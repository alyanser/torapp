#include "peer_wire_client.hxx"
#include "tcp_socket.hxx"

#include <QHostAddress>
#include <QUrl>

[[nodiscard]]
QByteArray Peer_wire_client::craft_request_message(const std::uint32_t index,const std::uint32_t offset,const std::uint32_t length) noexcept {
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
QByteArray Peer_wire_client::craft_cancel_message(const std::uint32_t index,const std::uint32_t offset,const std::uint32_t length) noexcept {
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
QByteArray Peer_wire_client::craft_have_message(const std::uint32_t piece_index) noexcept {
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

	assert(handshake_message.size() == 136);

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

QByteArray Peer_wire_client::craft_bitfield_message(const QBitArray & bitfield) noexcept {
	assert(bitfield.size() % 8 == 0);

	QByteArray bitfield_message = [&bitfield]{
		const auto length = 1 + static_cast<std::uint32_t>(bitfield.size()) / 8;
		return util::conversion::convert_to_hex(length);
	}();

	bitfield_message += []{
		constexpr auto bitfield_message_id = static_cast<std::uint8_t>(Message_Id::Bitfield);
		return util::conversion::convert_to_hex(bitfield_message_id);
	}();

	assert(bitfield_message.size() == 10);
	bitfield_message += util::conversion::convert_to_hex_bytes(bitfield);

	return bitfield_message;
}

void Peer_wire_client::do_handshake(const std::vector<QUrl> & peer_urls) const noexcept {

	for(const auto & peer_url : peer_urls){
		auto socket = std::make_shared<Tcp_socket>(peer_url)->bind_lifetime();

		connect(socket.get(),&Tcp_socket::connected,this,[&handshake_message_ = handshake_message_,socket = socket.get()]{
			socket->send_packet(handshake_message_);
		});

		connect(socket.get(),&Tcp_socket::readyRead,this,[this,socket = socket.get()]{

			try {
				if(socket->handshake_done()){
					communicate_with_peer(socket);
				}else{
					if(auto peer_info_opt = verify_handshake_response(socket)){
						auto & [peer_info_hash,peer_id] = peer_info_opt.value();

						if(info_sha1_hash_ == peer_info_hash){
							socket->set_handshake_done(true);
							socket->set_peer_id(std::move(peer_id));

							// if(received_pieces_){
								socket->send_packet(craft_bitfield_message(bitfield_));
							// }

							return;
						}
					}

					socket->disconnectFromHost();
				}

			}catch(const std::exception & exception){
				qDebug() << exception.what();
				socket->disconnectFromHost();
			}
		});
	}
}

[[nodiscard]]
std::optional<std::pair<QByteArray,QByteArray>> Peer_wire_client::verify_handshake_response(Tcp_socket * const socket){
	constexpr auto expected_response_length = 68;
	const auto response = socket->read(expected_response_length);

	{
		const auto protocol_label_len = [&response = response]{
			constexpr auto protocol_label_len_offset = 0;
			return util::extract_integer<std::uint8_t>(response,protocol_label_len_offset);
		}();

		if(constexpr auto expected_protocol_label_len = 19;protocol_label_len != expected_protocol_label_len){
			return {};
		}

		const auto protocol_label = [&response = response,protocol_label_len]{
			constexpr auto protocol_label_offset = 1;
			return response.sliced(protocol_label_offset,protocol_label_len);
		}();

		if(constexpr std::string_view expected_protocol("BitTorrent protocol");protocol_label.data() != expected_protocol){
			return {};
		}
	}

	{
		const auto reserved_bytes_content = [&response = response]{
			constexpr auto reserved_bytes_offset = 20;
			return util::extract_integer<std::uint32_t>(response,reserved_bytes_offset);
		}();

		if(reserved_bytes_content){
			return {};
		}
	}

	auto peer_info_hash = [&response = response]{
		constexpr auto sha1_hash_offset = 28;
		constexpr auto sha1_hash_length = 20;

		return response.sliced(sha1_hash_offset,sha1_hash_length).toHex();
	}();

	auto peer_id = [&response = response]{
		constexpr auto peer_id_offset = 48;
		constexpr auto peer_id_length = 20;
		return response.sliced(peer_id_offset,peer_id_length).toHex();
	}();

	return std::make_pair(std::move(peer_info_hash),std::move(peer_id));
}

void Peer_wire_client::communicate_with_peer(Tcp_socket * const socket) const {
	assert(socket->handshake_done());

	auto response_opt = socket->receive_packet();

	if(!response_opt.has_value()){
		return;
	}

	auto & [response_length,response] = response_opt.value();

	assert(response_length && response_length == response.size());
	
	const auto message_id = [&response = response]{
		constexpr auto message_id_offset = 0;
		return static_cast<Message_Id>(util::extract_integer<std::uint8_t>(response,message_id_offset));
	}();

	qInfo() << message_id;

	constexpr auto message_offset = 1;
	const auto payload_length = response_length - 1;

	auto extract_piece_metadata = [&response = response,payload_length]{

		if(constexpr auto standard_metadata_length = 12;payload_length != standard_metadata_length){
			throw std::length_error("length of received piece metadata message is non-standard");
		}

		const auto piece_index = [&response = response]{
			return util::extract_integer<std::uint32_t>(response,message_offset);
		}();

		const auto piece_offset = [&response = response]{
			constexpr auto piece_begin_offset = 5;
			return util::extract_integer<std::uint32_t>(response,piece_begin_offset);
		}();

		const auto piece_length = [&response = response]{
			constexpr auto piece_length_offset = 9;
			return util::extract_integer<std::uint32_t>(response,piece_length_offset);
		}();

		return std::make_tuple(piece_index,piece_offset,piece_length);
	};

	constexpr auto status_modifier_length = 1;

	switch(message_id){
		case Message_Id::Choke : {

			if(response_length != status_modifier_length){
				throw std::length_error("length of received 'Choke' message is non-standard");
			}
			
			socket->set_peer_choked(true);
			break;
		}

		case Message_Id::Unchoke : {

			if(response_length != status_modifier_length){
				throw std::length_error("length of received 'Unchoke' message is non-standard");
			}

			socket->set_peer_choked(false);
			break;
		}

		case Message_Id::Interested : {

			if(response_length != status_modifier_length){
				throw std::length_error("length of received 'Interested' message is non-standard");
			}

			socket->set_peer_interested(true);
			break;
		}

		case Message_Id::Uninterested : {

			if(response_length != status_modifier_length){
				throw std::length_error("length of received 'Uninterested' message is non-standard");
			}

			socket->set_peer_interested(false);
			break;
		}

		case Message_Id::Have : {

			if(constexpr auto standard_have_length = 4;payload_length != standard_have_length){
				throw std::length_error("length of received 'Have' message is non-standard");
			}
			
			const auto verified_piece_index = util::extract_integer<std::uint32_t>(response,message_offset);

			if(!bitfield_[verified_piece_index]){
				qInfo() << "requesting piece #" << verified_piece_index;
				socket->send_packet(unchoke_message.data());
				socket->send_packet(interested_message.data());
				// socket->send_packet(craft_request_message(verified_piece_index,0,torrent_metadata_.piece_length));
			}

			break;
		}

		case Message_Id::Bitfield : {

			if(payload_length * 8 != bitfield_.size()){
				socket->disconnectFromHost();
			}else{
				const auto peer_bitfield = util::conversion::convert_to_bits(response.sliced(message_offset,payload_length));

				qInfo() << peer_bitfield.size() << bitfield_.size();

				for(std::ptrdiff_t bit_idx = 0;bit_idx < peer_bitfield.size();bit_idx++){

					if(!bitfield_[bit_idx]){
						//todo figure test
						socket->send_packet(unchoke_message.data());
						socket->send_packet(interested_message.data());
						// socket->send_packet(craft_request_message(bit_idx,0,torrent_metadata_.piece_length));
					}
				}
			}

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

			const auto received_piece_index = [&response = response]{
				return util::extract_integer<std::uint32_t>(response,message_offset);
			}();

			const auto received_piece_offset = [&response = response]{
				constexpr auto piece_begin_offset = 5;
				return util::extract_integer<std::uint32_t>(response,piece_begin_offset);
			}();

			const auto received_piece_content = [&response = response]{
				constexpr auto piece_content_offset = 9;
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
