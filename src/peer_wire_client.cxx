#include "peer_wire_client.hxx"
#include "tcp_socket.hxx"

#include <QHostAddress>
#include <QUrl>
#include <QByteArrayView>

[[nodiscard]]
QByteArray Peer_wire_client::craft_handshake_packet() const noexcept {

	QByteArray handshake_packet = []{
		constexpr std::uint8_t pstrlen = 19;
		constexpr std::string_view protocol("BitTorrent protocol");
		static_assert(protocol.size() == pstrlen);
		
		return util::conversion::convert_to_hex(pstrlen,sizeof(pstrlen)) + QByteArray(protocol.data()).toHex();
	}();

	handshake_packet += []{
		constexpr auto reserved_bytes = 8;
		return util::conversion::convert_to_hex(0x0,reserved_bytes);
	}();

	handshake_packet += info_sha1_hash_ + id_;

	return handshake_packet;
}

void Peer_wire_client::do_handshake(const std::vector<QUrl> & peer_urls) const noexcept {

	for(const auto & peer_url : peer_urls){
		auto socket = std::make_shared<Tcp_socket>(peer_url)->bind_lifetime();

		connect(socket.get(),&Tcp_socket::connected,this,[&handshake_packet_ = handshake_packet_,socket = socket.get()]{
			socket->send_packet(handshake_packet_);
		});

		connect(socket.get(),&Tcp_socket::readyRead,this,[this,socket = socket.get()]{

			try {
				if(socket->handshake_done()){
					communicate_with_peer(socket);
				}else{
					if(auto peer_info_opt = handle_handshake_response(socket)){
						auto & [peer_info_hash,peer_id] = peer_info_opt.value();
						//todo add the counter and increment
						socket->set_peer_info_hash(std::move(peer_info_hash));
						socket->set_peer_id(std::move(peer_id));
					}else{
						socket->disconnectFromHost();
					}
				}

			}catch(const std::exception & exception){
				qDebug() << exception.what();
				socket->disconnectFromHost();
			}
		});
	}
}

std::optional<std::pair<QByteArray,QByteArray>> Peer_wire_client::handle_handshake_response(Tcp_socket * const socket){
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

		if(reserved_bytes_content){
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

void Peer_wire_client::communicate_with_peer(Tcp_socket * socket) const {
	assert(socket->handshake_done());

	qInfo() << "here after the handshake";
	
	const auto response = socket->readAll();
	
	const auto packet_length = [&response]{
		constexpr auto length_offset = 0;
		return util::extract_integer<std::uint32_t>(response,length_offset);
	}();
	
	if(!packet_length){ // keep-alive packet
		socket->reset_disconnect_timer();
		return;
	}

	const auto message_id = [&response]{
		constexpr auto message_id_offset = 4;
		return static_cast<Message_Id>(util::extract_integer<std::uint8_t>(response,message_id_offset));
	}();

	qInfo() << message_id << packet_length;

	constexpr auto message_offset = 5;
	const auto payload_length = packet_length - 1;

	auto extract_piece_metadata = [&response,payload_length]{

		if(constexpr auto standard_metadata_length = 12;payload_length != standard_metadata_length){
			throw std::length_error("length of received piece metadata packet is non-standard");
		}

		const auto piece_index = [&response]{
			return util::extract_integer<std::uint32_t>(response,message_offset);
		}();

		const auto piece_offset = [&response]{
			constexpr auto piece_begin_offset = 9;
			return util::extract_integer<std::uint32_t>(response,piece_begin_offset);
		}();

		const auto piece_length = [&response]{
			constexpr auto piece_length_offset = 13;
			return util::extract_integer<std::uint32_t>(response,piece_length_offset);
		}();

		return std::tuple{piece_index,piece_offset,piece_length};
	};

	constexpr auto status_modifier_length = 1;

	switch(message_id){
		case Message_Id::Choke : {

			if(packet_length != status_modifier_length){
				throw std::length_error("length of received 'Choke' packet is non-standard");
			}
			
			socket->set_state(Tcp_socket::State::Choked);
			break;
		}

		case Message_Id::Unchoke : {

			if(packet_length != status_modifier_length){
				throw std::length_error("length of received 'Unchoke' packet is non-standard");
			}

			socket->set_state(Tcp_socket::State::Unchoked);
			break;
		}

		case Message_Id::Interested : {

			if(packet_length != status_modifier_length){
				throw std::length_error("length of received 'Interested' packet is non-standard");
			}

			socket->set_state(Tcp_socket::State::Interested);
			break;
		}

		case Message_Id::Uninterested : {

			if(packet_length != status_modifier_length){
				throw std::length_error("length of received 'Uninterested' packet is non-standard");
			}

			socket->set_state(Tcp_socket::State::Uninterested);
			break;
		}

		case Message_Id::Have : {
			if(constexpr auto standard_have_length = 4;payload_length != standard_have_length){
				throw std::length_error("length of received 'Have' packet is non-standard");
			}
			
			const auto verified_piece_index = util::extract_integer<std::uint32_t>(response,message_offset);
			break;
		}

		case Message_Id::Bitfield : {
			//todo bitarray
			break;
		}

		case Message_Id::Request : {
			const auto [requested_piece_index,requested_piece_offset,requested_piece_length] = extract_piece_metadata();
			// min should be 16kb and max should be 128kb
			break;
		}

		case Message_Id::Piece : {

			if(constexpr auto min_piece_length = 8;payload_length < min_piece_length){
				throw std::length_error("length of received 'Piece' packet is non-standard");
			}

			const auto received_piece_index = [&response]{
				return util::extract_integer<std::uint32_t>(response,message_offset);
			}();

			const auto received_piece_offset = [&response]{
				constexpr auto piece_begin_offset = 9;
				return util::extract_integer<std::uint32_t>(response,piece_begin_offset);
			}();

			const auto received_piece_content = [&response]{
				constexpr auto piece_content_offset = 13;
				return response.sliced(piece_content_offset);
			}();

			break;
		}

		case Message_Id::Cancel : {
			const auto [cancelled_piece_index,cancelled_piece_offset,cancelled_piece_length] = extract_piece_metadata();
			break;
		}
	}
}