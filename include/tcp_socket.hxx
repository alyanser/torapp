#pragma once

#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include "utility.hxx"

class Tcp_socket : public QTcpSocket, public std::enable_shared_from_this<Tcp_socket> {
	Q_OBJECT
public:
	explicit Tcp_socket(QUrl peer_url);

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

	std::optional<std::pair<std::uint32_t,QByteArray>> receive_packet();

	QByteArray peer_id() const noexcept;
	void set_peer_id(QByteArray peer_id) noexcept;

	QUrl peer_url() const noexcept;
	void send_packet(const QByteArray & packet);
	void reset_disconnect_timer() noexcept;
	void add_pending_piece(std::uint32_t pending_piece_idx) noexcept;
	const QSet<std::uint32_t> & pending_pieces() const noexcept;
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
};

inline Tcp_socket::Tcp_socket(QUrl peer_url) : peer_url_(std::move(peer_url)){
	configure_default_connections();
	connectToHost(QHostAddress(peer_url_.host()),static_cast<std::uint16_t>(peer_url_.port()));
	reset_disconnect_timer();
}

inline std::shared_ptr<Tcp_socket> Tcp_socket::bind_lifetime() noexcept {
	connect(this,&QTcpSocket::disconnected,this,[self = shared_from_this()]{},Qt::SingleShotConnection);
	return shared_from_this();
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

constexpr void Tcp_socket::set_am_interested(bool am_interested) noexcept {
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

inline std::optional<std::pair<std::uint32_t,QByteArray>> Tcp_socket::receive_packet(){
	assert(handshake_done_);

	const auto message_length = [this]{
		const auto length_buffer = read(sizeof(std::uint32_t));
		constexpr auto length_offset = 0;
		return util::extract_integer<std::uint32_t>(length_buffer,length_offset);
	}();

	if(!message_length){ // keep alive packet
		return {};
	}

	if(auto message = read(message_length);message.size() == message_length){
		assert(!message.isEmpty());
		return std::make_pair(message_length,std::move(message));
	}

	return {};
}

constexpr void Tcp_socket::set_peer_choked(const bool peer_choked) noexcept {
	peer_choked_ = peer_choked;
}

inline void Tcp_socket::set_peer_id(QByteArray peer_id) noexcept {
	peer_id_ = std::move(peer_id);
}

inline void Tcp_socket::send_packet(const QByteArray & packet){
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

	disconnect_timer_.callOnTimeout([this]{
		qInfo() << "disconnecting from host due to timer";
		disconnectFromHost();
	});;
}

inline void Tcp_socket::reset_disconnect_timer() noexcept {
	constexpr std::chrono::minutes standard_disconnect_timeout(2);
	disconnect_timer_.start(standard_disconnect_timeout);
}

inline void Tcp_socket::add_pending_piece(const std::uint32_t pending_piece_idx) noexcept {
	pending_pieces_.insert(pending_piece_idx);
}

inline const QSet<std::uint32_t> & Tcp_socket::pending_pieces() const noexcept {
	return pending_pieces_;
}