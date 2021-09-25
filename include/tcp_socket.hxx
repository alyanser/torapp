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

	Tcp_socket(QUrl peer_url,QByteArray handshake_packet);

	std::shared_ptr<Tcp_socket> bind_lifetime() noexcept;

	constexpr void set_state(State state) noexcept;
	constexpr State state() const noexcept;

	void reset_timeout_timer() noexcept;
private:
	void configure_default_connections() noexcept;
	///
	QTimer timeout_timer_;
	QUrl peer_url_;
	QByteArray handshake_packet_;
	State state_ = State::Choked;
};

inline Tcp_socket::Tcp_socket(QUrl peer_url,QByteArray handshake_packet) : peer_url_(std::move(peer_url)),
	handshake_packet_(std::move(handshake_packet))
{
	configure_default_connections();
	connectToHost(QHostAddress(peer_url_.host()),static_cast<std::uint16_t>(peer_url_.port()));

	timeout_timer_.start(std::chrono::minutes(2));
}

inline std::shared_ptr<Tcp_socket> Tcp_socket::bind_lifetime() noexcept {

	connect(this,&QTcpSocket::disconnected,this,[self = shared_from_this()]{
	},Qt::SingleShotConnection);

	return shared_from_this();
}

constexpr void Tcp_socket::set_state(const State state) noexcept {
	state_ = state;
}

constexpr Tcp_socket::State Tcp_socket::state() const noexcept {
	return state_;
}

inline void Tcp_socket::configure_default_connections() noexcept {

	connect(this,&Tcp_socket::connected,[this]{
		write(handshake_packet_);
	});

	connect(&timeout_timer_,&QTimer::timeout,[this]{
		disconnectFromHost();
	});
}

inline void Tcp_socket::reset_timeout_timer() noexcept {
	constexpr std::chrono::minutes disconnect_timeout(2);
	timeout_timer_.start(disconnect_timeout);
}