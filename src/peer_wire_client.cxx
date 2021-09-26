#include "peer_wire_client.hxx"
#include "tcp_socket.hxx"

#include <QHostAddress>
#include <QUrl>

bool Peer_wire_client::validate_bittorrent_protocol(const QByteArray & response) noexcept {

	if(constexpr auto min_response_size = 25;response.size() < min_response_size){
		return false;
	}

	const auto protocol_label_len = [&response]{
		constexpr auto protocol_label_len_offset = 0;
		constexpr auto protocol_label_len_bytes = 1;

		return response.sliced(protocol_label_len_offset,protocol_label_len_bytes).toHex().toUInt(nullptr,hex_base);
	}();

	if(constexpr auto expected_protocol_label_len = 19;protocol_label_len != expected_protocol_label_len){
		return false;
	}

	const auto protocol_label = [&response,protocol_label_len]{
		constexpr auto protocol_label_offset = 1;
		return response.sliced(protocol_label_offset,protocol_label_len);
	}();

	if(constexpr std::string_view expected_protocol_label("BitTorrent protocol");protocol_label.data() != expected_protocol_label){
		return false;
	}

	const auto reserved_bytes = [&response]{
		constexpr auto reserved_bytes_offset = 21;
		constexpr auto reserved_bytes_len = 4;

		return response.sliced(reserved_bytes_offset,reserved_bytes_len).toHex().toUInt(nullptr,hex_base);
	}();

	return !reserved_bytes;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_handshake_packet() noexcept {

	QByteArray handshake_packet = []{
		constexpr std::uint8_t pstrlen = 19;
		constexpr std::string_view protocol("BitTorrent protocol");
		static_assert(protocol.size() == pstrlen);
		
		return util::conversion::convert_to_hex(pstrlen,sizeof(pstrlen)) + QByteArray(protocol.data()).toHex();
	}();

	handshake_packet += []{
		constexpr auto reserved_bytes = 8;
		return util::conversion::convert_to_hex(0,reserved_bytes);
	}();

	handshake_packet += info_sha1_hash_ + peer_id_;

	return handshake_packet;
}

void Peer_wire_client::do_handshake(const std::vector<QUrl> & peer_urls) noexcept {

	for(const auto & peer_url : peer_urls){
		auto socket = std::make_shared<Tcp_socket>(peer_url)->bind_lifetime();

		connect(socket.get(),&Tcp_socket::connected,this,[&handshake_packet_ = handshake_packet_,socket = socket.get()]{
			socket->send_packet(handshake_packet_);
		});

		connect(socket.get(),&Tcp_socket::readyRead,this,[this,socket = socket.get()]{
			on_socket_ready_read(socket);
		});
	}
}

void Peer_wire_client::on_socket_ready_read(Tcp_socket * const socket) noexcept {
	const auto response = socket->readAll();

	if(!validate_bittorrent_protocol(response)){
		return;
	}

	constexpr auto fixed_bytes = 4;

	const auto packet_length = [&response]{
		constexpr auto length_offset = 26;
		return response.sliced(length_offset,fixed_bytes).toHex().toUInt(nullptr,hex_base);
	}();

	if(!packet_length){
		socket->reset_disconnect_timer();
		return;
	}

	const auto message_id = [&response]{
		constexpr auto message_id_offset = 30;
		constexpr auto message_id_byte = 1;

		return static_cast<Message_Id>(response.sliced(message_id_offset,message_id_byte).toHex().toUShort(nullptr,hex_base));
	}();

	switch(message_id){
		case Message_Id::Choke : {
			socket->set_state(Tcp_socket::State::Choked);
			break;
		}

		case Message_Id::Unchoke : {
			socket->set_state(Tcp_socket::State::Unchoked);
			break;
		}

		case Message_Id::Interested : {
			socket->set_state(Tcp_socket::State::Interested);
			break;
		}

		case Message_Id::Uninterested : {
			socket->set_state(Tcp_socket::State::Uninterested);
			break;
		}

		case Message_Id::Have : {
			//5 
			const auto piece_index = [&response]{
				constexpr auto pieces_index_offset = 5;
				return response.sliced(pieces_index_offset,fixed_bytes).toHex().toUInt(nullptr,hex_base);
			}();

			break;
		}

		case Message_Id::Bitfield : {
			// based on the length

			break;
		}

		case Message_Id::Request : {

			const auto payload_index = [&response]{
				constexpr auto payload_index_offset = 5;
				return response.sliced(payload_index_offset,fixed_bytes).toHex().toUInt(nullptr,hex_base);
			}();

			const auto payload_begin = [&response]{
				constexpr auto payload_begin_offset = 9;
				return response.sliced(payload_begin_offset,fixed_bytes).toHex().toUInt(nullptr,hex_base);
			}();

			const auto payload_length = [&response]{
				constexpr auto payload_length_offset = 13;
				return response.sliced(payload_length_offset,fixed_bytes).toHex().toUInt(nullptr,hex_base);
			}();

			break;
		}

		case Message_Id::Piece : {
			//9 index begin block
			break;
		}

		case Message_Id::Cancel : {
			//13 index begin length
			break;
		}
	}
}