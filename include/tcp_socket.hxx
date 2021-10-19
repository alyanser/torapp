#pragma once

#include "util.hxx"

#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <algorithm>

class Tcp_socket : public QTcpSocket {
         Q_OBJECT
public:
         explicit Tcp_socket(QUrl peer_url,QObject * parent = nullptr);

         std::optional<std::pair<std::int32_t,QByteArray>> receive_packet() noexcept;
         void reset_disconnect_timer() noexcept;
         void send_packet(const QByteArray & packet) noexcept;
         QUrl peer_url() const noexcept;
         void on_invalid_peer_reply() noexcept;
         constexpr bool is_sound_ratio() const noexcept;
         constexpr void add_uploaded_bytes(std::int64_t uled_byte_cnt) noexcept;
         constexpr void add_downloaded_bytes(std::int64_t dled_byte_cnt) noexcept;
         constexpr double ratio() const noexcept;
         ///
         QBitArray peer_bitfield;
         QByteArray peer_id;
         QSet<std::int32_t> peer_allowed_fast_set;
         QSet<std::int32_t> allowed_fast_set;
         bool handshake_done = false;
         bool am_choking = true;
         bool peer_choked = true;
         bool am_interested = false;
         bool peer_interested = false;
         bool fast_extension_enabled = false;
signals:
         void got_choked() const;
         void ratio_changed(double old_ratio) const;
private:
         void configure_default_connections() noexcept;
         constexpr void update_ratio() noexcept;
         ///
         std::pair<std::optional<std::int32_t>,QByteArray> receive_buffer_;
         QTimer disconnect_timer_;
         QUrl peer_url_;
         double ratio_ = 0;
         std::int64_t dled_byte_cnt_ = 0;
         std::int64_t uled_byte_cnt_ = 0;
         std::int8_t peer_error_cnt_ = 0;
};

inline Tcp_socket::Tcp_socket(const QUrl peer_url,QObject * const parent)
         : QTcpSocket(parent)
         , peer_url_(peer_url)
{
         configure_default_connections();
         connectToHost(QHostAddress(peer_url_.host()),static_cast<std::uint16_t>(peer_url_.port()));
         
         disconnect_timer_.setSingleShot(true);
}

[[nodiscard]]
inline std::optional<std::pair<std::int32_t,QByteArray>> Tcp_socket::receive_packet() noexcept {
         auto & msg_size = receive_buffer_.first;

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

         auto & buffer_data = receive_buffer_.second;

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
         assert(!packet.isEmpty());

         if(state() == SocketState::ConnectedState){
                  write(QByteArray::fromHex(packet));
         }else{
                  qDebug() << "trying to send packet in disconnected state";
         }
}

[[nodiscard]]
inline QUrl Tcp_socket::peer_url() const noexcept {
         return peer_url_;
}

inline void Tcp_socket::on_invalid_peer_reply() noexcept {
         
         if(constexpr auto peer_error_threshold = 5;++peer_error_cnt_ > peer_error_threshold){
                  qDebug() << "Peer made too many mistakes. aborting";
                  abort();
         }
}

[[nodiscard]]
constexpr bool Tcp_socket::is_sound_ratio() const noexcept {
         constexpr auto min_ratio = 0.5;
         constexpr auto uled_byte_threshold = 1048576; // 1 mb
         return ratio_ >= min_ratio ? true : uled_byte_cnt_ < uled_byte_threshold;
}

constexpr void Tcp_socket::add_uploaded_bytes(const std::int64_t uled_byte_cnt) noexcept {
         assert(uled_byte_cnt > 0);
         uled_byte_cnt_ += uled_byte_cnt;
         update_ratio();
}

constexpr void Tcp_socket::add_downloaded_bytes(const std::int64_t dled_byte_cnt) noexcept {
         assert(dled_byte_cnt > 0);
         dled_byte_cnt_ += dled_byte_cnt;
         update_ratio();
}

[[nodiscard]]
constexpr double Tcp_socket::ratio() const noexcept {
         return ratio_;
}

inline void Tcp_socket::configure_default_connections() noexcept {
         connect(this,&Tcp_socket::disconnected,this,&Tcp_socket::deleteLater);
         connect(this,&Tcp_socket::readyRead,this,&Tcp_socket::reset_disconnect_timer);

         connect(this,&Tcp_socket::connected,[&disconnect_timer_ = disconnect_timer_]{
                  disconnect_timer_.start(std::chrono::minutes(2));
         });

         disconnect_timer_.callOnTimeout(this,[this]{
                  qDebug() << "connection timed out" << peer_id;
                  state() == SocketState::UnconnectedState ? deleteLater() : disconnectFromHost();
         });
}

constexpr void Tcp_socket::update_ratio() noexcept {
         assert(uled_byte_cnt_ >= 0);
         assert(dled_byte_cnt_ >= 0);

         const auto old_ratio = ratio_;
         ratio_ = static_cast<double>(dled_byte_cnt_) / static_cast<double>(uled_byte_cnt_ ? uled_byte_cnt_ : 1);
         emit ratio_changed(old_ratio);
}

inline void Tcp_socket::reset_disconnect_timer() noexcept {
         disconnect_timer_.start(std::chrono::minutes(10));
}