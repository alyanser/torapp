#include "peer_wire_client.hxx"
#include "tcp_socket.hxx"

void Peer_wire_client::do_handshake(const std::vector<QUrl> & peer_urls) const noexcept {

	for(const auto & peer_url : peer_urls){

		if(active_peers_.contains(peer_url)){
			continue;
		}

		active_peers_.insert(peer_url);

		auto socket = std::make_shared<Tcp_socket>(peer_url)->bind_lifetime();

		connect(socket.get(),&Tcp_socket::connected,this,[&handshake_message_ = handshake_message_,socket = socket.get()]{
			socket->send_packet(handshake_message_);
		});

		connect(socket.get(),&Tcp_socket::disconnected,this,[peer_url,&active_peers_ = active_peers_,socket]{
			[[maybe_unused]] const auto remove_success = active_peers_.remove(peer_url);
			assert(remove_success);
		});

		connect(socket.get(),&Tcp_socket::readyRead,this,[this,socket = socket.get()]{
			on_socket_ready_read(socket);
		});
	}
}


void Peer_wire_client::on_socket_ready_read(Tcp_socket * const socket) const noexcept {
	socket->reset_disconnect_timer();

	try {
		while(socket->bytesAvailable()){

			if(socket->handshake_done()){
				communicate_with_peer(socket);
			}else if(auto peer_info = verify_handshake_response(socket)){
				auto & [peer_info_hash,peer_id] = *peer_info;

				if(info_sha1_hash_ == peer_info_hash){
					socket->set_handshake_done(true);
					socket->set_peer_id(std::move(peer_id));

					if(downloaded_pieces_count_){
						socket->send_packet(craft_bitfield_message(bitfield_));
					}
				}else{
					qInfo() << "Client's hash doesn't match";
					socket->disconnectFromHost();
					break;
				}
			}else{
				socket->disconnectFromHost();
				break;
			}
		}

	}catch(const std::exception & exception){
		qDebug() << exception.what();
		socket->disconnectFromHost();
	}
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_request_message(const std::uint32_t piece_idx,const std::uint32_t block_idx) const noexcept {
	using util::conversion::convert_to_hex;

	auto packet_request_message = []{
		constexpr auto message_size = 13;
		return convert_to_hex(message_size);
	}();

	packet_request_message += []{
		constexpr auto request_packet_id = static_cast<std::uint8_t>(Message_Id::Request);
		return convert_to_hex(request_packet_id);
	}();

	packet_request_message += convert_to_hex(piece_idx);
	packet_request_message += convert_to_hex(block_idx);
	packet_request_message += convert_to_hex(get_block_size(piece_idx,block_idx));

	assert(packet_request_message.size() == 34);
	return packet_request_message;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_cancel_message(const std::uint32_t piece_idx,const std::uint32_t block_idx) noexcept {
	//todo exactly the same as request message except the id
	using util::conversion::convert_to_hex;

	auto packet_cancel_message = []{
		constexpr auto message_size = 13;
		return convert_to_hex(message_size);
	}();

	packet_cancel_message += []{
		constexpr auto cancel_packet_id = static_cast<std::uint8_t>(Message_Id::Cancel);
		return convert_to_hex(cancel_packet_id);
	}();

	packet_cancel_message += convert_to_hex(piece_idx);
	packet_cancel_message += convert_to_hex(block_idx);
	// todo get the piece size
	// packet_cancel_message += convert_to_hex(si);

	assert(packet_cancel_message.size() == 34);
	return packet_cancel_message;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_have_message(const std::uint32_t piece_idx) noexcept {
	using util::conversion::convert_to_hex;

	auto packet_have_message = []{
		constexpr auto message_size = 5;
		return convert_to_hex(message_size);
	}();

	packet_have_message += []{
		constexpr auto have_packet_id = static_cast<std::uint8_t>(Message_Id::Have);
		return convert_to_hex(have_packet_id);
	}();

	packet_have_message += convert_to_hex(piece_idx);
	
	assert(packet_have_message.size() == 18);
	return packet_have_message;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_handshake_message() const noexcept {

	QByteArray handshake_message = []{
		constexpr std::uint8_t pstrlen = 19;
		constexpr std::string_view protocol("BitTorrent protocol");
		static_assert(protocol.size() == pstrlen);
		
		return util::conversion::convert_to_hex(pstrlen) + QByteArray(protocol.data(),protocol.size()).toHex();
	}();

	handshake_message += []{
		constexpr std::uint64_t reserved_bytes_content = 0;
		return util::conversion::convert_to_hex(reserved_bytes_content);
	}();

	handshake_message += info_sha1_hash_ + id_;
	
	return handshake_message;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_piece_message(const std::uint32_t piece_idx,const std::uint32_t block_idx,const QByteArray & content) noexcept {
	using util::conversion::convert_to_hex;

	auto piece_message = [&content]{
		//! check if the size is fine
		const auto message_size = 9 + static_cast<std::uint32_t>(content.size());
		return convert_to_hex(message_size);
	}();
	
	piece_message += []{
		const auto piece_message_id = static_cast<std::uint8_t>(Message_Id::Piece);
		return convert_to_hex(piece_message_id);
	}();

	piece_message += convert_to_hex(piece_idx);
	piece_message += convert_to_hex(block_idx);
	piece_message += content.toHex();
	
	return piece_message;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_bitfield_message(const QBitArray & bitfield) noexcept {

	QByteArray bitfield_message = [&bitfield]{
		assert(bitfield.size() % 8 == 0);
		const auto size = 1 + static_cast<std::uint32_t>(bitfield.size()) / 8;
		return util::conversion::convert_to_hex(size);
	}();

	bitfield_message += []{
		constexpr auto bitfield_message_id = static_cast<std::uint8_t>(Message_Id::Bitfield);
		return util::conversion::convert_to_hex(bitfield_message_id);
	}();

	assert(bitfield_message.size() == 10);
	bitfield_message += util::conversion::convert_to_hex_bytes(bitfield);

	return bitfield_message;
}

[[nodiscard]]
std::optional<std::pair<QByteArray,QByteArray>> Peer_wire_client::verify_handshake_response(Tcp_socket * const socket){
	constexpr auto expected_response_size = 68;
	const auto response = socket->read(expected_response_size);

	if(response.size() != expected_response_size){
		return {};
	}

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

		// todo support fast connect extension here

		if(reserved_bytes_content){
			return {};
		}
	}

	auto peer_info_hash = [&response = response]{
		constexpr auto sha1_hash_offset = 28;
		constexpr auto sha1_hash_size = 20;
		return response.sliced(sha1_hash_offset,sha1_hash_size).toHex();
	}();

	auto peer_id = [&response = response]{
		constexpr auto peer_id_offset = 48;
		constexpr auto peer_id_size = 20;
		return response.sliced(peer_id_offset,peer_id_size).toHex();
	}();

	return std::make_pair(std::move(peer_info_hash),std::move(peer_id));
}

 void Peer_wire_client::on_unchoke_message_received(Tcp_socket * const socket) const noexcept {
	socket->set_peer_choked(false);

	if(!socket->pending_pieces().isEmpty()){
		const auto pending_piece_idx = *socket->pending_pieces().begin();

		for(std::uint64_t block_idx = 0;block_idx < std::min(average_blocks_count_,5UL);block_idx++){

			if(!block_present(pending_piece_idx,static_cast<std::uint32_t>(block_idx))){
				socket->send_packet(craft_request_message(pending_piece_idx,static_cast<std::uint32_t>(block_idx)));
			}
		}
	}
}

 void Peer_wire_client::on_have_message_received(Tcp_socket * const socket,const QByteArray & response) const noexcept {
	constexpr auto message_begin_offset = 1;
	const auto peer_have_piece_idx = util::extract_integer<std::uint32_t>(response,message_begin_offset);

	if(!bitfield_[peer_have_piece_idx]){
		socket->set_am_interested(true);

		if(socket->peer_choked()){
			socket->add_pending_piece(peer_have_piece_idx);
		}else{
			// todo loop through and send the request for all subsets
			socket->send_packet(craft_request_message(peer_have_piece_idx,0));
		}
	}
}

 void Peer_wire_client::on_bitfield_received(Tcp_socket * socket,const QByteArray & response,const std::uint32_t payload_size) const noexcept {
 	
	if(static_cast<std::ptrdiff_t>(payload_size * 8) != bitfield_.size()){
		qInfo() << "Invalid bitfield size";
		socket->disconnectFromHost();
		return;
	}

	constexpr auto message_begin_offset = 1;
	const auto peer_bitfield = util::conversion::convert_to_bits(response.sliced(message_begin_offset,payload_size));

	for(std::ptrdiff_t bit_idx = 0;bit_idx < peer_bitfield.size();bit_idx++){

		if(peer_bitfield[bit_idx] && !bitfield_[bit_idx]){
			socket->set_am_interested(true);
			socket->add_pending_piece(static_cast<std::uint32_t>(bit_idx));
		}
	}

	if(socket->am_interested()){
		socket->send_packet(interested_message.data());
	}
 }

 void Peer_wire_client::on_piece_received(const QByteArray & response) const noexcept {
	constexpr auto message_begin_offset = 1;
	const auto received_piece_idx = util::extract_integer<std::uint32_t>(response,message_begin_offset);

	const auto received_block_idx = [&response = response]{
		constexpr auto piece_begin_offset = 5;
		return util::extract_integer<std::uint32_t>(response,piece_begin_offset);
	}();

	const auto received_content = [&response = response]{
		constexpr auto piece_content_offset = 9;
		return response.sliced(piece_content_offset);
	}();

	if(received_piece_idx >= total_pieces_count_ || received_block_idx >= average_blocks_count_){
		return;
	}

	auto & [piece_metadata,piece] = pieces_[received_piece_idx];
	auto & [received_blocks_count,blocks_status] = piece_metadata;

	if(piece.isEmpty()){
		assert(blocks_status.empty());
		piece.resize(static_cast<std::ptrdiff_t>(piece_size_));
		//todo change the size after setting up helper
		blocks_status.resize(average_blocks_count_);
	}

	assert(!blocks_status[received_block_idx]);

	received_blocks_count++;
	blocks_status[received_block_idx] = true;

	std::memcpy(&piece[received_block_idx],received_content.data(),static_cast<std::size_t>(received_content.size()));

	// todo change after the getter
	if(received_blocks_count == average_blocks_count_){
		//todo verify the hash
		//todo send packet from all the sockets about have and update the bitfield
		downloaded_pieces_count_++;
	}
	
	qInfo() << "stored the piece" << received_piece_idx << received_block_idx;
 }

void Peer_wire_client::communicate_with_peer(Tcp_socket * const socket) const {
	assert(socket->handshake_done());
	
	auto response_opt = socket->receive_packet();

	if(!response_opt){
		return;
	}

	auto & [response_size,response] = *response_opt;

	assert(response_size && response_size == response.size());
	
	const auto message_id = [&response = response]{
		constexpr auto message_id_offset = 0;
		return static_cast<Message_Id>(util::extract_integer<std::uint8_t>(response,message_id_offset));
	}();
	

	auto extract_piece_metadata = [&response = response]{

		const auto piece_index = [&response = response]{
			constexpr auto message_begin_offset = 1;
			return util::extract_integer<std::uint32_t>(response,message_begin_offset);
		}();

		const auto piece_offset = [&response = response]{
			constexpr auto piece_begin_offset = 5;
			return util::extract_integer<std::uint32_t>(response,piece_begin_offset);
		}();

		const auto piece_size = [&response = response]{
			constexpr auto piece_size_offset = 9;
			return util::extract_integer<std::uint32_t>(response,piece_size_offset);
		}();

		return std::make_tuple(piece_index,piece_offset,piece_size);
	};

	switch(message_id){
		case Message_Id::Choke : {
			socket->set_peer_choked(true);
			break;
		}

		case Message_Id::Unchoke : {
			on_unchoke_message_received(socket);
			break;
		}

		case Message_Id::Interested : {
			socket->set_peer_interested(true);
			break;
		}

		case Message_Id::Uninterested : {
			socket->set_peer_interested(false);
			break;
		}

		case Message_Id::Have : {
			on_have_message_received(socket,response);
			break;
		}

		case Message_Id::Bitfield : {
			on_bitfield_received(socket,response,response_size - 1);
			break;
		}

		case Message_Id::Request : {
			const auto [requested_piece_idx,requested_piece_offset,requested_piece_size] = extract_piece_metadata();
			// min should be 16kb and max should be 128kb
			break;
		}

		case Message_Id::Piece : {
			on_piece_received(response);
			break;
		}

		case Message_Id::Cancel : {
			const auto [cancelled_piece_idx,cancelled_piece_offset,cancelled_piece_size] = extract_piece_metadata();
			break;
		}
	}
}