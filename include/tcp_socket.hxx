#pragma once

#include <QTcpSocket>
#include <QUrl>
#include <memory>
#include <QHostAddress>

class Tcp_socket : public QTcpSocket, public std::enable_shared_from_this<Tcp_socket> {
	Q_OBJECT
public:
	Tcp_socket(QUrl peer_url,QByteArray handshake_packet);
	std::shared_ptr<Tcp_socket> bind_lifetime() noexcept;
private:
	void configure_default_connections() noexcept;
	///
	QUrl peer_url_;
	QByteArray handshake_packet_;
};

inline Tcp_socket::Tcp_socket(QUrl peer_url,QByteArray handshake_packet) : peer_url_(std::move(peer_url)),
	handshake_packet_(std::move(handshake_packet))
{
	configure_default_connections();
	connectToHost(QHostAddress(peer_url_.host()),static_cast<std::uint16_t>(peer_url_.port()));
}

inline std::shared_ptr<Tcp_socket> Tcp_socket::bind_lifetime() noexcept {

	connect(this,&QTcpSocket::disconnected,this,[self = shared_from_this()]{
	},Qt::SingleShotConnection);

	return shared_from_this();
}

inline void Tcp_socket::configure_default_connections() noexcept {

	connect(this,&Tcp_socket::connected,[this]{
		write(handshake_packet_);
	});

	connect(this,&Tcp_socket::readyRead,[this]{
		qInfo() << "reading packet";
	});
}