#include "peer_wire_client.hxx"
#include "tcp_socket.hxx"

#include <QHostAddress>
#include <QUrl>

[[nodiscard]]
QByteArray Peer_wire_client::craft_packet_request_message(const std::uint32_t index,const std::uint32_t offset,const std::uint32_t length) noexcept {
	using util::conversion::convert_to_hex;

	auto packet_request_message = []{
		constexpr auto message_length = 13;
		return convert_to_hex(message_length);
	}();

	packet_request_message += []{
		constexpr auto request_packet_id = static_cast<std::uint8_t>(Message_Id::Request);
		return convert_to_hex(request_packet_id);
	}();

	packet_request_message += convert_to_hex(index);
	packet_request_message += convert_to_hex(offset);
	packet_request_message += convert_to_hex(length);

	assert(packet_request_message.size() == 34);
	return packet_request_message;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_packet_cancel_message(const std::uint32_t index,const std::uint32_t offset,const std::uint32_t length) noexcept {
	//todo exactly the same as request message except the id 
	using util::conversion::convert_to_hex;

	auto packet_cancel_message = []{
		constexpr auto message_length = 13;
		return convert_to_hex(message_length);
	}();

	packet_cancel_message += []{
		constexpr auto cancel_packet_id = static_cast<std::uint8_t>(Message_Id::Cancel);
		return convert_to_hex(cancel_packet_id);
	}();

	packet_cancel_message += convert_to_hex(index);
	packet_cancel_message += convert_to_hex(offset);
	packet_cancel_message += convert_to_hex(length);

	assert(packet_cancel_message.size() == 34);
	return packet_cancel_message;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_packet_have_message(const std::uint32_t piece_index) noexcept {
	using util::conversion::convert_to_hex;

	auto packet_have_message = []{
		constexpr auto message_length = 5;
		return convert_to_hex(message_length);
	}();

	packet_have_message += []{
		constexpr auto have_packet_id = static_cast<std::uint8_t>(Message_Id::Have);
		return convert_to_hex(have_packet_id);
	}();

	packet_have_message += convert_to_hex(piece_index);
	
	assert(packet_have_message.size() == 18);
	return packet_have_message;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_handshake_message() const noexcept {

	QByteArray handshake_message = []{
		constexpr std::uint8_t pstrlen = 19;
		constexpr std::string_view protocol("BitTorrent protocol");
		static_assert(protocol.size() == pstrlen);
		
		return util::conversion::convert_to_hex(pstrlen) + QByteArray(protocol.data()).toHex();
	}();

	handshake_message += []{
		constexpr std::uint64_t reserved_bytes_content = 0;
		return util::conversion::convert_to_hex(reserved_bytes_content);
	}();

	handshake_message += info_sha1_hash_ + id_;

	return handshake_message;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_piece_message(const std::uint32_t index,const std::uint32_t offset,const QByteArray & content) noexcept {
	using util::conversion::convert_to_hex;

	auto piece_message = [&content]{
		//! check if the size is fine
		const auto message_length = 9 + static_cast<std::uint32_t>(content.size());
		return convert_to_hex(message_length);
	}();
	
	piece_message += []{
		const auto piece_message_id = static_cast<std::uint8_t>(Message_Id::Piece);
		return convert_to_hex(piece_message_id);
	}();

	piece_message += convert_to_hex(index);
	piece_message += convert_to_hex(offset);
	piece_message += content.toHex();
	
	return piece_message;
}

void Peer_wire_client::do_handshake(const std::vector<QUrl> & peer_urls) const noexcept {

	for(const auto & peer_url : peer_urls){
		auto socket = std::make_shared<Tcp_socket>(peer_url)->bind_lifetime();

		connect(socket.get(),&Tcp_socket::connected,this,[&handshake_message_ = handshake_message_,socket = socket.get()]{
			socket->send_packet(handshake_message_);
		});

		connect(socket.get(),&Tcp_socket::readyRead,this,[socket = socket.get()]{

			try {
				if(socket->handshake_done()){
					communicate_with_peer(socket);
				}else{
					if(auto peer_info_opt = verify_handshake_response(socket)){
						auto & [peer_info_hash,peer_id] = peer_info_opt.value();

						socket->set_handshake_done(true);
						socket->set_peer_info_hash(std::move(peer_info_hash));
						socket->set_peer_id(std::move(peer_id));

						{
							socket->send_packet(keep_alive_message_.data());
						}
					}else{
						socket->disconnectFromHost();
					}
				}

			}catch(const std::exception & exception){
				socket->disconnectFromHost();
			}
		});
	}
}

[[nodiscard]]
std::optional<std::pair<QByteArray,QByteArray>> Peer_wire_client::verify_handshake_response(Tcp_socket * const socket){
	const auto response = socket->readAll();

	{
		const auto protocol_label_len = [&response]{
			constexpr auto protocol_label_len_offset = 0;
			return util::extract_integer<std::uint8_t>(response,protocol_label_len_offset);
		}();

		if(constexpr auto expected_protocol_label_len = 19;protocol_label_len != expected_protocol_label_len){
			return {};
		}

		const auto protocol_label = [response,protocol_label_len]{
			constexpr auto protocol_label_offset = 1;
			return response.sliced(protocol_label_offset,protocol_label_len);
		}();

		if(constexpr std::string_view expected_protocol("BitTorrent protocol");protocol_label.data() != expected_protocol){
			return {};
		}
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

void Peer_wire_client::communicate_with_peer(Tcp_socket * const socket){
	assert(socket->handshake_done());

	const auto response = socket->readAll();
	
	const auto message_length = [&response]{
		constexpr auto length_offset = 0;
		return util::extract_integer<std::uint32_t>(response,length_offset);
	}();
	
	if(!message_length){ // keep-alive message
		socket->reset_disconnect_timer();
		return;
	}

	const auto message_id = [&response]{
		constexpr auto message_id_offset = 4;
		return static_cast<Message_Id>(util::extract_integer<std::uint8_t>(response,message_id_offset));
	}();

	qInfo() << message_id;

	constexpr auto message_offset = 5;
	const auto payload_length = message_length - 1;

	auto extract_piece_metadata = [&response,payload_length]{

		if(constexpr auto standard_metadata_length = 12;payload_length != standard_metadata_length){
			throw std::length_error("length of received piece metadata message is non-standard");
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

			if(message_length != status_modifier_length){
				throw std::length_error("length of received 'Choke' message is non-standard");
			}
			
			socket->set_state(Tcp_socket::State::Choked);
			break;
		}

		case Message_Id::Unchoke : {

			if(message_length != status_modifier_length){
				throw std::length_error("length of received 'Unchoke' message is non-standard");
			}

			socket->set_state(Tcp_socket::State::Unchoked);
			break;
		}

		case Message_Id::Interested : {

			if(message_length != status_modifier_length){
				throw std::length_error("length of received 'Interested' message is non-standard");
			}

			socket->set_state(Tcp_socket::State::Interested);
			break;
		}

		case Message_Id::Uninterested : {

			if(message_length != status_modifier_length){
				throw std::length_error("length of received 'Uninterested' message is non-standard");
			}

			socket->set_state(Tcp_socket::State::Uninterested);
			break;
		}

		case Message_Id::Have : {
			
			if(constexpr auto standard_have_length = 4;payload_length != standard_have_length){
				throw std::length_error("length of received 'Have' message is non-standard");
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
				throw std::length_error("length of received 'Piece' message is non-standard");
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