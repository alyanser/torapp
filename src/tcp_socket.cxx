#include "tcp_socket.hxx"

#include <QHostAddress>

Tcp_socket::Tcp_socket(QUrl peer_url,const std::int64_t uled_byte_threshold,QObject * const parent)
         : QTcpSocket(parent)
         , uled_byte_threshold_(uled_byte_threshold)
         , peer_url_(std::move(peer_url))
{
         configure_default_connections();
         connectToHost(QHostAddress(peer_url_.host()),static_cast<std::uint16_t>(peer_url_.port()));
         
         disconnect_timer_.setSingleShot(true);
         request_timer_.setInterval(std::chrono::milliseconds(10));
}

[[nodiscard]]
std::optional<QByteArray> Tcp_socket::receive_packet() noexcept {
         auto & msg_size = receive_buffer_.first;

         if(!handshake_done && !msg_size){
                  constexpr auto protocol_handshake_msg_size = 68;
                  msg_size = protocol_handshake_msg_size;
         }

         if(!msg_size){
                  constexpr auto msg_len_byte_cnt = 4;

                  if(bytesAvailable() < msg_len_byte_cnt){
                           return {};
                  }

                  startTransaction();
                  const auto size_buffer = read(msg_len_byte_cnt);
                  assert(size_buffer.size() == msg_len_byte_cnt);

                  try{
                           msg_size = util::extract_integer<std::int32_t>(size_buffer,0);
                  }catch(const std::exception & exception){
                           qDebug() << exception.what();
                           rollbackTransaction();
                           assert(!msg_size);
                           return {};
                  }
                  
                  commitTransaction();
         }

         assert(msg_size);

         if(!*msg_size){ // keep alive packet
                  qDebug() << "Keep alive packet";
                  msg_size.reset();
                  return {};
         }

         auto & buffer_data = receive_buffer_.second;

         if((buffer_data.capacity() < *msg_size)){
                  buffer_data.reserve(*msg_size);
         }

         if(buffer_data += read(*msg_size - buffer_data.size());buffer_data.size() == *msg_size){
                  assert(!buffer_data.isEmpty());
                  msg_size.reset();
                  return std::move(buffer_data);
         }

         assert(buffer_data.size() < *msg_size);
         return {};
}

[[nodiscard]]
bool Tcp_socket::is_good_ratio() const noexcept {
         constexpr auto min_ratio = 0.5;
         assert(uled_byte_cnt_ >= 0 && dled_byte_cnt_ >= 0);
         return uled_byte_cnt_ <= uled_byte_threshold_ ? true : !uled_byte_cnt_ || static_cast<double>(dled_byte_cnt_) / static_cast<double>(uled_byte_cnt_) >= min_ratio;
}

void Tcp_socket::send_packet(const QByteArray & packet) noexcept {
         assert(!packet.isEmpty());

         if(state() == SocketState::ConnectedState){
                  write(QByteArray::fromHex(packet));
         }
}

void Tcp_socket::configure_default_connections() noexcept {
         connect(this,&Tcp_socket::disconnected,this,&Tcp_socket::deleteLater);
         connect(this,&Tcp_socket::readyRead,this,&Tcp_socket::reset_disconnect_timer);

         connect(this,&Tcp_socket::connected,[&disconnect_timer_ = disconnect_timer_]{
                  disconnect_timer_.start(std::chrono::minutes(2));
         });

         disconnect_timer_.callOnTimeout(this,[this]{
                  state() == SocketState::UnconnectedState ? deleteLater() : disconnectFromHost();
         });

         request_timer_.callOnTimeout(this,[this]{

                  if(requests_.isEmpty()){
                           request_timer_.stop();
                  }else{
                           send_packet(*requests_.begin());
                           requests_.erase(requests_.begin());
                  }
         });
}