#include "peer_wire_client.hxx"
#include "tcp_socket.hxx"

#include <QHostAddress>
#include <QUrl>

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

	handshake_packet += info_sha1_hash_ + id_;

	return handshake_packet;
}

void Peer_wire_client::do_handshake(const std::vector<QUrl> & peer_urls) noexcept {

	for(const auto & peer_url : peer_urls){
		auto socket = std::make_shared<Tcp_socket>(peer_url)->bind_lifetime();

		connect(socket.get(),&Tcp_socket::connected,this,[&handshake_packet_ = handshake_packet_,socket = socket.get()]{
			socket->send_packet(handshake_packet_);
		});

		connect(socket.get(),&Tcp_socket::readyRead,this,[this,socket = socket.get()]{

			if(socket->handshake_done()){
				communicate_with_peer(socket);
			}else{
				if(auto peer_info_opt = handle_handshake_response(socket)){
					auto & [peer_info_hash,peer_id] = peer_info_opt.value();

					socket->set_peer_info_hash(std::move(peer_info_hash));
					socket->set_peer_id(std::move(peer_id));
				}else{
					socket->disconnectFromHost();
				}
			}
		});
	}
}

std::optional<std::pair<QByteArray,QByteArray>> Peer_wire_client::handle_handshake_response(Tcp_socket * const socket) noexcept {
	const auto response = socket->readAll();

	const auto protocol_label_len = [&response]{
		constexpr auto protocol_label_len_offset = 0;
		return util::extract_integer<std::uint8_t>(response,protocol_label_len_offset);
	}();

	if(constexpr auto expected_protocol_label_len = 19;protocol_label_len != expected_protocol_label_len){
		return {};
	}

	const auto protocol_label = [&response,protocol_label_len]{
		constexpr auto protocol_label_offset = 1;
		return response.sliced(protocol_label_offset,protocol_label_len);
	}();

	if(constexpr std::string_view expected_protocol_label("BitTorrent protocol");protocol_label.data() != expected_protocol_label){
		return {};
	}

	{
		const auto reserved_bytes_content = [&response]{
			constexpr auto reserved_bytes_offset = 20;
			return util::extract_integer<std::uint32_t>(response,reserved_bytes_offset);
		}();

		if(reserved_bytes_content){ // only support 0x0000
			return {};
		}
	}

	if(constexpr auto expected_response_size = 68;response.size() != expected_response_size){
		return {};
	}

	auto peer_info_hash = [&response]{
		constexpr auto sha1_hash_offset = 28;
		constexpr auto sha1_hash_length = 20;

		return response.sliced(sha1_hash_offset,sha1_hash_length).toHex();
	}();

	auto peer_id = [&response]{
		constexpr auto peer_id_offset = 48;
		constexpr auto peer_id_length = 20;

		return response.sliced(peer_id_offset,peer_id_length).toHex();
	}();

	return std::make_pair(std::move(peer_info_hash),std::move(peer_id));
}

void Peer_wire_client::communicate_with_peer(Tcp_socket * socket) noexcept {
	assert(socket->handshake_done());
	qInfo() << "here after the handshake";
	const auto response = socket->readAll();
	constexpr auto fixed_bytes = 4;
	
	const auto packet_length = [&response]{
		constexpr auto length_offset = 24;
		return util::extract_integer<std::uint32_t>(response,length_offset);
	}();
	
	if(!packet_length){
		socket->reset_disconnect_timer();
		return;
	}

	const auto message_id = [&response]{
		constexpr auto message_id_offset = 28;
		return static_cast<Message_Id>(util::extract_integer<std::uint8_t>(response,message_id_offset));
	}();

	qInfo() << message_id << packet_length;

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
				return util::extract_integer<std::uint32_t>(response,pieces_index_offset);
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
				return util::extract_integer<std::uint32_t>(response,payload_index_offset);
			}();

			const auto payload_begin = [&response]{
				constexpr auto payload_begin_offset = 9;
				return util::extract_integer<std::uint32_t>(response,payload_begin_offset);
			}();

			const auto payload_length = [&response]{
				constexpr auto payload_length_offset = 13;
				return util::extract_integer<std::uint32_t>(response,payload_length_offset);
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