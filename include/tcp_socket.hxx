#pragma once

#include "utility.hxx"

#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

class Tcp_socket : public QTcpSocket {
         Q_OBJECT
public:
         explicit Tcp_socket(QUrl peer_url,QObject * parent = nullptr);

         constexpr bool handshake_done() const noexcept;
         constexpr void set_handshake_done(bool handshake_status) noexcept;

         constexpr void set_am_choking(bool am_choking) noexcept;
         constexpr void set_peer_choked(bool peer_choked) noexcept;
         constexpr bool am_choking() const noexcept;
         constexpr bool peer_choked() const noexcept;

         constexpr void set_am_interested(bool am_interested) noexcept;
         constexpr void set_peer_interested(bool peer_interested) noexcept;
         constexpr bool am_interested() const noexcept;
         constexpr bool peer_interested() const noexcept;

         constexpr bool fast_ext_enabled() const noexcept;
         constexpr void set_fast_ext_enabled(bool fast_ext_enabled) noexcept;

         std::optional<std::pair<std::uint32_t,QByteArray>> receive_packet();

         const QByteArray & peer_id() const noexcept;
         void set_peer_id(QByteArray peer_id) noexcept;

         void set_peer_bitfield(QBitArray peer_bitfield) noexcept;
         QBitArray & peer_bitfield() noexcept;

         QSet<std::uint32_t> & pending_pieces() noexcept;
         void send_packet(const QByteArray & packet);
         void reset_disconnect_timer() noexcept;
         void add_pending_piece(std::uint32_t pending_piece_idx) noexcept;
signals:
         void got_choked() const;
         void request_rejected() const;
         void fast_have_msg_received(std::uint32_t peer_have_piece_idx) const;
private:
         void configure_default_connections() noexcept;
         ///
         QBitArray peer_bitfield_;
         QSet<std::uint32_t> pending_pieces_;
         QByteArray peer_id_;
         QTimer disconnect_timer_;
         QUrl peer_url_;
         bool handshake_done_ = false;
         bool am_choking_ = true;
         bool peer_choked_ = true;
         bool am_interested_ = false;
         bool peer_interested_ = false;
         bool fast_ext_enabled_ = false;
};

inline Tcp_socket::Tcp_socket(const QUrl peer_url,QObject * const parent) 
         : QTcpSocket(parent)
         , peer_url_(peer_url)
{
         configure_default_connections();
         connectToHost(QHostAddress(peer_url_.host()),static_cast<std::uint16_t>(peer_url_.port()));
         reset_disconnect_timer();

         disconnect_timer_.setSingleShot(true);
}

constexpr void Tcp_socket::set_am_choking(const bool am_choking) noexcept {
         am_choking_ = am_choking;
}

[[nodiscard]]
constexpr bool Tcp_socket::am_choking() const noexcept {
         return am_choking_;
}

[[nodiscard]]
constexpr bool Tcp_socket::peer_choked() const noexcept {
         return peer_choked_;
}

constexpr void Tcp_socket::set_am_interested(const bool am_interested) noexcept {
         am_interested_ = am_interested;
}

constexpr void Tcp_socket::set_peer_interested(const bool peer_interested) noexcept {
         peer_interested_ = peer_interested;
}

[[nodiscard]]
constexpr bool Tcp_socket::am_interested() const noexcept {
         return am_interested_;
}

[[nodiscard]]
constexpr bool Tcp_socket::peer_interested() const noexcept {
         return peer_interested_;
}

[[nodiscard]]
constexpr bool Tcp_socket::fast_ext_enabled() const noexcept {
         return fast_ext_enabled_;
}

constexpr void Tcp_socket::set_fast_ext_enabled(const bool fast_ext_enabled) noexcept {
         fast_ext_enabled_ = fast_ext_enabled;
}

[[nodiscard]]
inline std::optional<std::pair<std::uint32_t,QByteArray>> Tcp_socket::receive_packet(){

         if(constexpr auto minimum_response_size = 4;bytesAvailable() < minimum_response_size){
                  return {};
         }

         reset_disconnect_timer();

         assert(handshake_done_);
         startTransaction();
         
         const auto msg_size = [this]() -> std::optional<std::uint32_t> {
                  constexpr auto len_byte_cnt = 4;
                  const auto size_buffer = read(len_byte_cnt);

                  if(size_buffer.size() < len_byte_cnt){
                           return {};
                  }

                  constexpr auto size_offset = 0;

                  return util::extract_integer<std::uint32_t>(size_buffer,size_offset);
         }();

         if(!msg_size){
                  qInfo() << "couldn't have 4 bytes even";
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

constexpr void Tcp_socket::set_peer_choked(const bool peer_choked) noexcept {
         peer_choked_ = peer_choked;
}

inline void Tcp_socket::set_peer_id(QByteArray peer_id) noexcept {
         peer_id_ = std::move(peer_id);
}

inline void Tcp_socket::send_packet(const QByteArray & packet){
         assert(state() == QAbstractSocket::SocketState::ConnectedState);
         assert(!packet.isEmpty());
         write(QByteArray::fromHex(packet));
}

[[nodiscard]]
inline const QByteArray & Tcp_socket::peer_id() const noexcept {
         return peer_id_;
}

[[nodiscard]]
constexpr bool Tcp_socket::handshake_done() const noexcept {
         return handshake_done_;
}

constexpr void Tcp_socket::set_handshake_done(const bool handshake_status) noexcept {
         handshake_done_ = handshake_status;
}

inline void Tcp_socket::configure_default_connections() noexcept {
         connect(this,&Tcp_socket::disconnected,this,&Tcp_socket::deleteLater);
         connect(this,&Tcp_socket::readyRead,this,&Tcp_socket::reset_disconnect_timer);

         disconnect_timer_.callOnTimeout(this,[this]{
                  qInfo() << "disconnecting from peer due to connection timeout";
                  state() == SocketState::ConnectedState ? disconnectFromHost() : deleteLater();
         });
}

inline void Tcp_socket::reset_disconnect_timer() noexcept {
         disconnect_timer_.start(std::chrono::minutes(5));
}

inline void Tcp_socket::add_pending_piece(const std::uint32_t pending_piece_idx) noexcept {
         pending_pieces_.insert(pending_piece_idx);
}

[[nodiscard]]
inline QBitArray & Tcp_socket::peer_bitfield() noexcept {
         return peer_bitfield_;
}

inline void Tcp_socket::set_peer_bitfield(QBitArray peer_bitfield) noexcept {
         peer_bitfield_ = std::move(peer_bitfield);
}

[[nodiscard]]
inline QSet<std::uint32_t> & Tcp_socket::pending_pieces() noexcept {
         return pending_pieces_;
}