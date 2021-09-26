#pragma once

#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

class Tcp_socket : public QTcpSocket, public std::enable_shared_from_this<Tcp_socket> {
	Q_OBJECT

public:
	enum class State {
		Choked,
		Unchoked,
		Interested,
		Uninterested
	};

	explicit Tcp_socket(QUrl peer_url);

	std::shared_ptr<Tcp_socket> bind_lifetime() noexcept;

	constexpr bool handshake_done() const noexcept;
	constexpr void set_handshake_done(bool handshake_status) noexcept;

	constexpr State state() const noexcept;
	constexpr void set_state(State state) noexcept;

	QByteArray peer_id() const noexcept;
	void set_peer_id(QByteArray peer_id) noexcept;

	QByteArray peer_info_hash() const noexcept;
	void set_peer_info_hash(QByteArray peer_info_hash) noexcept;
	
	QUrl peer_url() const noexcept;
	void send_packet(const QByteArray & packet);
	void reset_disconnect_timer() noexcept;
private:
	void configure_default_connections() noexcept;
	///
	QByteArray peer_id_;
	QByteArray peer_info_hash_;
	QTimer disconnect_timer_;
	State state_ = State::Choked;
	QUrl peer_url_;
	bool handshake_done_ = false;
};

inline Tcp_socket::Tcp_socket(QUrl peer_url) : peer_url_(std::move(peer_url)){
	configure_default_connections();
	connectToHost(QHostAddress(peer_url_.host()),static_cast<std::uint16_t>(peer_url_.port()));

	disconnect_timer_.start(std::chrono::minutes(2));
}

inline std::shared_ptr<Tcp_socket> Tcp_socket::bind_lifetime() noexcept {

	connect(this,&QTcpSocket::disconnected,this,[self = shared_from_this()]{
	},Qt::SingleShotConnection);

	return shared_from_this();
}

inline void Tcp_socket::set_peer_id(QByteArray peer_id) noexcept {
	peer_id_ = std::move(peer_id);
}

inline void Tcp_socket::send_packet(const QByteArray & packet){
	const auto raw_fmt = QByteArray::fromHex(packet);
	write(raw_fmt.data(),raw_fmt.size());
}

[[nodiscard]]
inline QByteArray Tcp_socket::peer_id() const noexcept {
	return peer_id_;
}

[[nodiscard]]
inline QByteArray Tcp_socket::peer_info_hash() const noexcept {
	return peer_info_hash_;
}

inline void Tcp_socket::set_peer_info_hash(QByteArray peer_info_hash) noexcept {
	peer_info_hash_ = std::move(peer_info_hash);
}

[[nodiscard]]
constexpr bool Tcp_socket::handshake_done() const noexcept {
	return handshake_done_;
}

constexpr void Tcp_socket::set_handshake_done(const bool handshake_status) noexcept {
	handshake_done_ = handshake_status;
}

constexpr void Tcp_socket::set_state(const State state) noexcept {
	state_ = state;
}

[[nodiscard]]
constexpr Tcp_socket::State Tcp_socket::state() const noexcept {
	return state_;
}

[[nodiscard]]
inline QUrl Tcp_socket::peer_url() const noexcept {
	return peer_url_;
}

inline void Tcp_socket::configure_default_connections() noexcept {

	connect(&disconnect_timer_,&QTimer::timeout,[this]{
		disconnectFromHost();
	});
}

inline void Tcp_socket::reset_disconnect_timer() noexcept {
	constexpr std::chrono::minutes disconnect_timeout(2);
	disconnect_timer_.start(disconnect_timeout);
}