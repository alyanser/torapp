#pragma once

#include <QNetworkAccessManager>

class Peer_wire_client : public QNetworkAccessManager, std::enable_shared_from_this<Peer_wire_client> {
	Q_OBJECT
public:
	std::shared_ptr<Peer_wire_client> bind_lifetime() noexcept;
};

inline std::shared_ptr<Peer_wire_client> Peer_wire_client::bind_lifetime() noexcept {
	return shared_from_this();
}