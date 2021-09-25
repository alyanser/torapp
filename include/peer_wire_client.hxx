#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>

class Peer_wire_client : public QNetworkAccessManager, public std::enable_shared_from_this<Peer_wire_client> {
	Q_OBJECT
public:
	std::shared_ptr<Peer_wire_client> bind_lifetime() noexcept;
	void connect(const std::vector<QUrl> & peer_urls) noexcept;
};

inline std::shared_ptr<Peer_wire_client> Peer_wire_client::bind_lifetime() noexcept {
	return shared_from_this();
}