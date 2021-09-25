#include "peer_wire_client.hxx"
#include "tcp_socket.hxx"

#include <QHostAddress>
#include <QUrl>

QByteArray Peer_wire_client::craft_handshake_packet() noexcept {
	QByteArray handshake_packet;

	handshake_packet += []{
		constexpr uint8_t pstrlen = 19;
		constexpr std::string_view protocol("Bittorrent protocol");

		static_assert(protocol.size() == pstrlen);

		return util::conversion::convert_to_hex(pstrlen,sizeof(pstrlen)) + QByteArray(protocol.data()).toHex();
	}();

	handshake_packet += []{
		constexpr auto reserved_bytes = 8;
		return util::conversion::convert_to_hex(0,reserved_bytes);
	}();

	handshake_packet += info_sha1_hash_;
	handshake_packet += peer_id_;
	
	return handshake_packet;
}

void Peer_wire_client::do_handshake(const std::vector<QUrl> & peer_urls) noexcept {

	for(const auto & peer_url : peer_urls){
		auto socket = std::make_shared<Tcp_socket>(peer_url,handshake_packet_)->bind_lifetime();
	}
}