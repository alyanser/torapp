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

	void send_packet(const QByteArray & packet);
	constexpr void set_state(State state) noexcept;
	constexpr State state() const noexcept;
	QUrl peer_url() const noexcept;
	void reset_disconnect_timer() noexcept;
private:
	void configure_default_connections() noexcept;
	///
	QTimer disconnect_timer_;
	State state_ = State::Choked;
	QUrl peer_url_;
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