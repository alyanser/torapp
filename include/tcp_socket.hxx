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

	std::shared_ptr<Tcp_socket> bind_lifetime() noexcept;

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

	QByteArray peer_id() const noexcept;
	void set_peer_id(QByteArray peer_id) noexcept;

	QUrl peer_url() const noexcept;
	void send_packet(const QByteArray & packet);
	void reset_disconnect_timer() noexcept;
	void add_pending_piece(std::uint32_t pending_piece_idx) noexcept;
	QSet<std::uint32_t> & pending_pieces() noexcept;
signals:
	void got_choked() const;
	void shutdown() const;
private:
	void configure_default_connections() noexcept;
	///
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

inline Tcp_socket::Tcp_socket(QUrl peer_url,QObject * const parent) : QTcpSocket(parent), peer_url_(std::move(peer_url)){
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

constexpr bool Tcp_socket::fast_ext_enabled() const noexcept {
	return fast_ext_enabled_;
}

constexpr void Tcp_socket::set_fast_ext_enabled(const bool fast_ext_enabled) noexcept {
	fast_ext_enabled_ = fast_ext_enabled;
}

inline std::optional<std::pair<std::uint32_t,QByteArray>> Tcp_socket::receive_packet(){
	assert(handshake_done_);

	const auto msg_size = [this]() -> std::optional<std::uint32_t> {
		const auto size_buffer = read(sizeof(std::uint32_t));

		if(static_cast<std::size_t>(size_buffer.size()) < sizeof(std::uint32_t)){
			qInfo() << "couldn't have 4 bytes even";
			return {};
		}

		constexpr auto size_offset = 0;
		return util::extract_integer<std::uint32_t>(size_buffer,size_offset);
	}();

	if(!msg_size){
		disconnectFromHost();
	}else if(*msg_size){ // keep alive packet
		return {};
	}

	if(auto msg = read(*msg_size);msg.size() == msg_size){
		assert(!msg.isEmpty());
		return std::make_pair(*msg_size,std::move(msg));
	}

	return {};
}

constexpr void Tcp_socket::set_peer_choked(const bool peer_choked) noexcept {
	peer_choked_ = peer_choked;

	if(peer_choked_){
		emit got_choked();
	}
}

inline void Tcp_socket::set_peer_id(QByteArray peer_id) noexcept {
	peer_id_ = std::move(peer_id);
}

inline void Tcp_socket::send_packet(const QByteArray & packet){
	assert(!packet.isEmpty());
	write(QByteArray::fromHex(packet));
}

[[nodiscard]]
inline QByteArray Tcp_socket::peer_id() const noexcept {
	return peer_id_;
}

[[nodiscard]]
constexpr bool Tcp_socket::handshake_done() const noexcept {
	return handshake_done_;
}

constexpr void Tcp_socket::set_handshake_done(const bool handshake_status) noexcept {
	handshake_done_ = handshake_status;
}

[[nodiscard]]
inline QUrl Tcp_socket::peer_url() const noexcept {
	return peer_url_;
}

inline void Tcp_socket::configure_default_connections() noexcept {
	connect(this,&Tcp_socket::disconnected,this,&Tcp_socket::deleteLater);
	connect(this,&Tcp_socket::readyRead,this,&Tcp_socket::reset_disconnect_timer);
	connect(&disconnect_timer_,&QTimer::timeout,this,&Tcp_socket::disconnectFromHost);
}

inline void Tcp_socket::reset_disconnect_timer() noexcept {
	constexpr std::chrono::minutes standard_disconnect_timeout(2);
	disconnect_timer_.start(standard_disconnect_timeout);
}

inline void Tcp_socket::add_pending_piece(const std::uint32_t pending_piece_idx) noexcept {
	pending_pieces_.insert(pending_piece_idx);
}

inline QSet<std::uint32_t> & Tcp_socket::pending_pieces() noexcept {
	return pending_pieces_;
}