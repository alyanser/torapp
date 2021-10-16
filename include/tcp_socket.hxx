#pragma once

#include "util.hxx"

#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

class Tcp_socket : public QTcpSocket {
         Q_OBJECT
public:
         explicit Tcp_socket(QUrl peer_url,QObject * parent = nullptr);

         void reset_disconnect_timer() noexcept;
         std::optional<std::pair<std::int32_t,QByteArray>> receive_packet() noexcept;
         void send_packet(const QByteArray & packet) noexcept;
         QUrl peer_url() const noexcept;
         void on_invalid_peer_reply() noexcept;
         ///
         QBitArray peer_bitfield;
         QByteArray peer_id;
         QSet<std::int32_t> pending_pieces;
         QSet<std::int32_t> fast_pieces;
         bool handshake_done = false;
         bool am_choking = true;
         bool peer_choked = true;
         bool am_interested = false;
         bool peer_interested = false;
         bool fast_ext_enabled = false;
signals:
         void got_choked() const;
         void request_rejected() const;
         void fast_have_msg_received(std::int32_t peer_have_piece_idx) const;
private:
         void configure_default_connections() noexcept;
         ///
         std::pair<std::optional<std::int32_t>,QByteArray> buffer_;
         QTimer disconnect_timer_;
         QUrl peer_url_;
         std::int8_t peer_error_cnt_ = 0;
};

inline Tcp_socket::Tcp_socket(const  QUrl peer_url,QObject * const parent) 
         : QTcpSocket(parent)
         , peer_url_(peer_url)
{
         configure_default_connections();
         connectToHost(QHostAddress(peer_url_.host()),static_cast<std::uint16_t>(peer_url_.port()));
         reset_disconnect_timer();

         disconnect_timer_.setSingleShot(true);
}

[[nodiscard]]
inline std::optional<std::pair<std::int32_t,QByteArray>> Tcp_socket::receive_packet() noexcept {
         auto & msg_size = buffer_.first;

         if(!handshake_done && !msg_size){
                  constexpr auto protocol_handshake_msg_size = 68;
                  msg_size = protocol_handshake_msg_size;
         }

         if(!msg_size){
                  constexpr auto len_byte_cnt = 4;

                  if(bytesAvailable() < len_byte_cnt){
                           return {};
                  }

                  startTransaction();
                  const auto size_buffer = read(len_byte_cnt);
                  assert(size_buffer.size() == len_byte_cnt);

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

         auto & buffer_data = buffer_.second;

         if(buffer_data.capacity() != *msg_size){
                  buffer_data.reserve(*msg_size);
         }

         assert(buffer_data.size() < *msg_size);

         if(buffer_data += read(*msg_size - buffer_data.size());buffer_data.size() == *msg_size){
                  assert(buffer_data.size());
                  const auto packet_size = *msg_size;
                  msg_size.reset();
                  return std::make_pair(packet_size,std::move(buffer_data));
         }

         return {};
}

inline void Tcp_socket::send_packet(const QByteArray & packet) noexcept {

         if(state() == SocketState::ConnectedState){
                  assert(!packet.isEmpty());
                  write(QByteArray::fromHex(packet));
         }
}

[[nodiscard]]
inline QUrl Tcp_socket::peer_url() const noexcept {
         return peer_url_;
}

inline void Tcp_socket::on_invalid_peer_reply() noexcept {
         
         if(constexpr auto peer_error_threshold = 5;++peer_error_cnt_ > peer_error_threshold){
                  abort();
         }
}

inline void Tcp_socket::configure_default_connections() noexcept {
         connect(this,&Tcp_socket::disconnected,this,&Tcp_socket::deleteLater);
         connect(this,&Tcp_socket::readyRead,this,&Tcp_socket::reset_disconnect_timer);

         disconnect_timer_.callOnTimeout(this,[this]{
                  qDebug() << "connection timed out" << peer_id;
                  state() == SocketState::UnconnectedState ? deleteLater() : disconnectFromHost();
         });
}

inline void Tcp_socket::reset_disconnect_timer() noexcept {
         disconnect_timer_.start(std::chrono::minutes(5));
}