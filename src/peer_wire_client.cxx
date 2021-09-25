#include "peer_wire_client.hxx"
#include <QHostAddress>

void Peer_wire_client::connect(const std::vector<QUrl> & peer_urls) noexcept {

	for(const auto & peer_url : peer_urls){
		assert(peer_url.isValid());
	}
}