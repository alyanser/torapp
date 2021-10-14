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
         std::optional<std::pair<std::int32_t,QByteArray>> receive_packet();
         void send_packet(const QByteArray & packet) noexcept;
         QUrl peer_url() const noexcept;
         ///
         QBitArray peer_bitfield;
         QByteArray peer_id;
         QSet<std::int32_t> pending_pieces;
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
         QTimer disconnect_timer_;
         QUrl peer_url_;
};

inline Tcp_socket::Tcp_socket(const QUrl peer_url,QObject * const parent) 
         : QTcpSocket(parent)
         , peer_url_(peer_url)
{
         configure_default_connections();
         connectToHost(QHostAddress(peer_url.host()),static_cast<std::uint16_t>(peer_url.port()));
         reset_disconnect_timer();

         disconnect_timer_.setSingleShot(true);
}

[[nodiscard]]
inline std::optional<std::pair<std::int32_t,QByteArray>> Tcp_socket::receive_packet(){

         if(constexpr auto min_response_size = 4;bytesAvailable() < min_response_size){
                  return {};
         }

         assert(handshake_done);

         reset_disconnect_timer();
         startTransaction();
         
         const auto msg_size = [this]() -> std::optional<std::int32_t> {
                  constexpr auto len_byte_cnt = 4;
                  const auto size_buffer = read(len_byte_cnt);

                  if(size_buffer.size() < len_byte_cnt){
                           return {};
                  }

                  constexpr auto size_offset = 0;
                  return util::extract_integer<std::int32_t>(size_buffer,size_offset);
         }();

         if(!msg_size){
                  rollbackTransaction();
                  return {};
         }

         if(!*msg_size){ // keep alive packet
                  commitTransaction();
                  return {};
         }

         if(auto msg = read(*msg_size);msg.size() == *msg_size){
                  commitTransaction();
                  return std::make_pair(*msg_size,std::move(msg));
         }

         rollbackTransaction();
         return {};
}

inline void Tcp_socket::send_packet(const QByteArray & packet) noexcept {

         if(state() == SocketState::ConnectedState){
                  assert(!packet.isEmpty());
                  write(QByteArray::fromHex(packet));
         }else{
                  qDebug() << "Tried to send packet in disconnected state";
         }
}

[[nodiscard]]
inline QUrl Tcp_socket::peer_url() const noexcept {
         return peer_url_;
}

inline void Tcp_socket::configure_default_connections() noexcept {
         connect(this,&Tcp_socket::disconnected,this,&Tcp_socket::deleteLater);
         connect(this,&Tcp_socket::readyRead,this,&Tcp_socket::reset_disconnect_timer);

         disconnect_timer_.callOnTimeout(this,[this]{
                  qDebug() << "connection timed out" << peer_id;
                  state() == SocketState::ConnectedState ? disconnectFromHost() : deleteLater();
         });
}

inline void Tcp_socket::reset_disconnect_timer() noexcept {
         disconnect_timer_.start(std::chrono::minutes(5));
}