#pragma once

#include "utility.hxx"

#include <QByteArray>
#include <QObject>
#include <QDebug>

class Peer_wire_client : public QObject, public std::enable_shared_from_this<Peer_wire_client> {
	Q_OBJECT
public:
	Peer_wire_client(QByteArray peer_id,QByteArray info_sha1_hash);

	std::shared_ptr<Peer_wire_client> bind_lifetime() noexcept;
	void do_handshake(const std::vector<QUrl> & peer_urls) noexcept;
private:
	QByteArray craft_handshake_packet() noexcept;
	///
	QByteArray peer_id_;
	QByteArray info_sha1_hash_;
	QByteArray handshake_packet_;
};

inline Peer_wire_client::Peer_wire_client(QByteArray peer_id,QByteArray info_sha1_hash) : peer_id_(std::move(peer_id)),
	info_sha1_hash_(std::move(info_sha1_hash)), handshake_packet_(craft_handshake_packet())
{
}

inline std::shared_ptr<Peer_wire_client> Peer_wire_client::bind_lifetime() noexcept {
	return shared_from_this();
}