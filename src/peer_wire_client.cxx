#include "peer_wire_client.hxx"
#include "tcp_socket.hxx"

[[nodiscard]]
bool Peer_wire_client::verify_hash(const std::size_t piece_idx,const QByteArray & received_packet) const noexcept {
         constexpr auto sha1_hash_byte_cnt = 20;

         assert(piece_idx < total_piece_cnt_ && piece_idx * sha1_hash_byte_cnt < metadata_.pieces.size());
         assert(metadata_.pieces.size() % sha1_hash_byte_cnt == 0);

         QByteArray piece_hash(metadata_.pieces.substr(piece_idx * sha1_hash_byte_cnt,sha1_hash_byte_cnt).data(),sha1_hash_byte_cnt);
         assert(piece_hash.size() % sha1_hash_byte_cnt == 20);
         
         return piece_hash == QCryptographicHash::hash(received_packet,QCryptographicHash::Sha1);
}

[[nodiscard]]
Peer_wire_client::Piece_metadata Peer_wire_client::get_piece_info(const std::uint32_t piece_idx,const std::uint32_t block_idx) const noexcept {
         const auto piece_size = piece_idx == total_piece_cnt_ - 1 && torrent_size_ % piece_size_ ? torrent_size_ % piece_size_ : piece_size_;
         const auto total_blocks = static_cast<std::uint32_t>(std::ceil(static_cast<double>(piece_size) / max_block_size));

         assert(block_idx < total_blocks);

         const auto block_size = static_cast<std::uint32_t>(block_idx == total_blocks - 1 && piece_size % max_block_size ? piece_size % max_block_size : max_block_size);

         return {static_cast<std::uint32_t>(piece_size),block_size,total_blocks};
}

void Peer_wire_client::do_handshake(const std::vector<QUrl> & peer_urls) noexcept {

         for(const auto & peer_url : peer_urls){

                  if(active_peers_.contains(peer_url)){
                           continue;
                  }

                  active_peers_.insert(peer_url);

                  auto * const socket = new Tcp_socket(peer_url,this);

                  connect(socket,&Tcp_socket::connected,this,[&handshake_msg_ = handshake_msg_,socket]{
                           assert(!socket->handshake_done());
                           socket->send_packet(handshake_msg_);
                  });

                  connect(socket,&Tcp_socket::readyRead,this,[this,socket]{
                           on_socket_ready_read(socket);
                  });

                  connect(this,&Peer_wire_client::piece_downloaded,socket,[socket](const std::uint32_t piece_idx){
                           qInfo() << "Sending have packet after verifying the piece";
                           socket->send_packet(craft_have_message(piece_idx));
                  });

                  connect(socket,&Tcp_socket::disconnected,this,[this,socket,peer_url]{
                           [[maybe_unused]] const auto remove_success = active_peers_.remove(peer_url);
                           assert(remove_success);

                           if(socket->handshake_done()){
                                    --active_connection_cnt_;
                                    qInfo() << "Active connection cnt" << active_connection_cnt_;
                           }
                  });
         }
}

void Peer_wire_client::on_socket_ready_read(Tcp_socket * const socket) noexcept {
         
         try {
                  if(socket->handshake_done()){
                           assert(!socket->peer_id().isEmpty());
                           communicate_with_peer(socket);
                  }else if(auto peer_info = verify_handshake_response(socket)){
                           auto & [peer_info_hash,peer_id] = *peer_info;

                           if(info_sha1_hash_ == peer_info_hash){
                                    ++active_connection_cnt_;
                                    socket->set_peer_id(std::move(peer_id));
                                    socket->set_handshake_done(true);

                                    qInfo() << "Active connection cnt" << active_connection_cnt_;

                                    if(remaining_pieces_.empty() && socket->fast_ext_enabled()){
                                             socket->send_packet(have_all_msg.data());
                                    }else if(static_cast<std::uint64_t>(remaining_pieces_.size()) < total_piece_cnt_){
                                             socket->send_packet(craft_bitfield_message(bitfield_));
                                    }else if(socket->fast_ext_enabled()){
                                             assert(static_cast<std::size_t>(remaining_pieces_.size()) == total_piece_cnt_);
                                             socket->send_packet(have_none_msg.data());
                                    }

                                    if(socket->bytesAvailable()){
                                             QTimer::singleShot(0,this,[this,socket]{ on_socket_ready_read(socket); });
                                    }
                           }else{
                                    qInfo() << "peer's hash doesn't match";
                                    socket->disconnectFromHost();
                           }
                  }else{
                           qInfo() << "Invalid handshake";
                           socket->disconnectFromHost();
                  }

         }catch(const std::exception & exception){
                  qDebug() << exception.what();
                  socket->disconnectFromHost();
         }
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_request_message(const std::uint32_t piece_idx,const std::uint32_t block_idx) const noexcept {
         using util::conversion::convert_to_hex;

         auto request_msg = []{
                  constexpr quint32_be request_msg_size(13);
                  return convert_to_hex(request_msg_size);
         }();

         request_msg += convert_to_hex(static_cast<std::uint8_t>(Message_Id::Request));
         request_msg += convert_to_hex(static_cast<quint32_be>(piece_idx));
         request_msg += convert_to_hex(static_cast<quint32_be>(block_idx));

         request_msg += convert_to_hex(static_cast<quint32_be>(get_piece_info(piece_idx,block_idx).block_size));

         assert(request_msg.size() == 34);
         return request_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_cancel_message(const std::uint32_t piece_idx,const std::uint32_t block_idx) const noexcept {
         //todo exactly the same as request message except the id
         using util::conversion::convert_to_hex;

         auto cancel_msg = []{
                  constexpr quint32_be cancel_msg_size(13);
                  return convert_to_hex(cancel_msg_size);
         }();

         cancel_msg += convert_to_hex(static_cast<std::uint8_t>(Message_Id::Cancel));
         cancel_msg += convert_to_hex(static_cast<quint32_be>(piece_idx));
         cancel_msg += convert_to_hex(static_cast<quint32_be>(block_idx));
         cancel_msg += convert_to_hex(get_piece_info(piece_idx,block_idx).block_size);

         assert(cancel_msg.size() == 34);
         return cancel_msg;
}

QByteArray Peer_wire_client::craft_allowed_fast_message(const std::uint32_t piece_idx) noexcept {
         using util::conversion::convert_to_hex;

         auto allowed_fast_msg = []{
                  constexpr quint32_be allowed_fast_msg_size(5);
                  return convert_to_hex(allowed_fast_msg_size);
         }();

         allowed_fast_msg += convert_to_hex(static_cast<std::uint8_t>(Message_Id::Allowed_Fast));
         allowed_fast_msg += convert_to_hex(static_cast<quint32_be>(piece_idx));
         assert(allowed_fast_msg.size() == 18);
         return allowed_fast_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_have_message(const std::uint32_t piece_idx) noexcept {
         using util::conversion::convert_to_hex;

         auto have_msg = []{
                  constexpr quint32_be have_msg_size(5);
                  return convert_to_hex(have_msg_size);
         }();

         have_msg += convert_to_hex(static_cast<std::uint8_t>(Message_Id::Have));
         have_msg += convert_to_hex(static_cast<quint32_be>(piece_idx));
         
         assert(have_msg.size() == 18);
         return have_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_handshake_message() const noexcept {

         QByteArray handshake_msg = []{
                  constexpr std::uint8_t pstrlen = 19;
                  constexpr std::string_view protocol("BitTorrent protocol");
                  static_assert(protocol.size() == pstrlen);
                  return util::conversion::convert_to_hex(pstrlen) + QByteArray(protocol.data(),protocol.size()).toHex();
         }();

         assert(handshake_msg.size() == 40);
         handshake_msg += reserved_byte + info_sha1_hash_ + id_;
         assert(handshake_msg.size() == 136);

         return handshake_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_piece_message(const std::uint32_t piece_idx,const std::uint32_t block_idx,const QByteArray & content) noexcept {
         using util::conversion::convert_to_hex;

         auto piece_msg = [content_size = content.size()]{
                  assert(content_size < std::numeric_limits<std::uint32_t>::max());
                  const quint32_be msg_size(9 + static_cast<std::uint32_t>(content_size));
                  return convert_to_hex(msg_size);
         }();
         
         piece_msg += convert_to_hex(static_cast<std::uint8_t>(Message_Id::Piece));
         piece_msg += convert_to_hex(static_cast<quint32_be>(piece_idx));
         piece_msg += convert_to_hex(static_cast<quint32_be>(block_idx));
         assert(piece_msg.size() == 18);
         piece_msg += content.toHex();
         
         return piece_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_bitfield_message(const QBitArray & bitfield) noexcept {

         QByteArray bitfield_msg = [&bitfield]{
                  assert(bitfield.size() % 8 == 0);
                  const quint32_be bitfield_msg_size(1 + static_cast<std::uint32_t>(bitfield.size()) / 8);
                  return util::conversion::convert_to_hex(bitfield_msg_size);
         }();

         bitfield_msg += util::conversion::convert_to_hex(static_cast<std::uint8_t>(Message_Id::Bitfield));
         assert(bitfield_msg.size() == 10);
         bitfield_msg += util::conversion::convert_to_hex_bytes(bitfield);

         return bitfield_msg;
}

[[nodiscard]]
std::optional<std::pair<QByteArray,QByteArray>> Peer_wire_client::verify_handshake_response(Tcp_socket * const socket){
         constexpr auto expected_response_size = 68;
         const auto response = socket->read(expected_response_size);

         if(response.size() != expected_response_size){
                  qInfo() << "Invalid peer handshake response size";
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
                           qInfo() << "Peer is using some woodoo protocol";
                           return {};
                  }
         }

         {
                  const auto peer_reserved_bytes = [&response = response]{
                           constexpr auto reserved_bytes_offset = 20;
                           constexpr auto reserved_byte_cnt = 8;
                           return response.sliced(reserved_bytes_offset,reserved_byte_cnt);
                  }();

                  const auto peer_reserved_bits = util::conversion::convert_to_bits(peer_reserved_bytes);
                  assert(peer_reserved_bytes.size() * 8 == peer_reserved_bits.size());

                  constexpr auto fast_ext_bit_idx = 61;

                  if(peer_reserved_bits[fast_ext_bit_idx]){
                           socket->set_fast_ext_enabled(true);
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

void Peer_wire_client::send_block_requests(Tcp_socket * const socket,const std::uint32_t piece_idx) noexcept {
         assert(socket->peer_bitfield()[piece_idx]);
         const auto total_blocks = get_piece_info(piece_idx,0).total_blocks;

         auto & [requested_blocks,received_blocks,piece,received_block_cnt] = pieces_[piece_idx];

         if(requested_blocks.empty()){
                  requested_blocks.resize(total_blocks);
         }

         if(received_blocks.empty()){
                  received_blocks.resize(total_blocks);
         }

         constexpr auto max_requests = 150;
         constexpr auto max_duplicate_requests = 3;

         for(std::uint32_t block_idx = 0,request_sent_cnt = 0;request_sent_cnt < max_requests &&block_idx < total_blocks;++block_idx){
                  assert(requested_blocks[block_idx] <= max_duplicate_requests);

                  if(!received_blocks[block_idx] && requested_blocks[block_idx] < max_duplicate_requests){
                           ++request_sent_cnt;
                           ++requested_blocks[block_idx];

                           connect(socket,&Tcp_socket::got_choked,this,[socket,block_request_cnt = requested_blocks[block_idx]]() mutable {

                                    if(!socket->fast_ext_enabled()){
                                             assert(block_request_cnt);
                                             --block_request_cnt;
                                    }
                           });

                           connect(socket,&Tcp_socket::request_rejected,this,[socket,block_request_cnt = requested_blocks[block_idx]]() mutable {
                                    assert(socket->fast_ext_enabled());
                                    assert(block_request_cnt);
                                    --block_request_cnt;
                           });

                           connect(socket,&Tcp_socket::disconnected,this,[socket,block_request_cnt = requested_blocks[block_idx]]() mutable {

                                    if(!socket->peer_choked()){
                                             assert(block_request_cnt);
                                             --block_request_cnt;
                                    }
                           });

                           socket->send_packet(craft_request_message(piece_idx,block_idx));
                  }
         }
}

[[nodiscard]]
bool Peer_wire_client::valid_response(Tcp_socket * const socket,const QByteArray & response,const Message_Id received_msg_id) noexcept {
         constexpr auto max_msg_id = 17;

         if(static_cast<std::uint32_t>(received_msg_id) > max_msg_id){
                  qInfo() << "peer sent unexpected id" << received_msg_id;
                  return false;
         }

         constexpr auto pseudo = 0;

         constexpr std::array<std::uint32_t,max_msg_id + 1> expected_response_sizes {
                  1, // choke
                  1, // unchoke
                  1, // interested
                  1, // uninterested
                  5, // have
                  pseudo,
                  9, // request
                  pseudo, // pseudo
                  9, // cancel
                  pseudo,pseudo,pseudo,pseudo,
                  5, // suggest piece
                  1, // have all
                  1, // have none
                  13, // reject response
                  5  // allowed fast
         };

         switch(received_msg_id){
                  case Message_Id::Bitfield : {
                           constexpr auto min_bitfield_msg_size = 1;
                           return response.size() >= min_bitfield_msg_size;
                  }

                  case Message_Id::Piece : {
                           constexpr auto min_bitfield_msg_size = 9;
                           return response.size() >= min_bitfield_msg_size;
                  }

                  default : {
                           const auto received_msg_idx = static_cast<std::size_t>(received_msg_id);
                           const auto expected_size = expected_response_sizes[static_cast<std::size_t>(received_msg_id)];
                           
                           if(expected_size == pseudo){
                                    return false;
                           }

                           if(received_msg_idx >= static_cast<std::uint32_t>(Message_Id::Suggest_Piece) && !socket->fast_ext_enabled()){
                                    qInfo() << "peer sent fast ext ids without enabling ext";
                                    return false;
                           }

                           return response.size() == expected_size;
                  }
         }

         __builtin_unreachable();
}

void Peer_wire_client::on_unchoke_message_received(Tcp_socket * const socket) noexcept {
         socket->set_peer_choked(false);

         if(socket->pending_pieces().contains(get_current_target_piece())){
                  send_block_requests(socket,get_current_target_piece());
         }
}

void Peer_wire_client::on_have_message_received(Tcp_socket * const socket,const std::uint32_t peer_have_piece_idx) noexcept {

         if(peer_have_piece_idx >= total_piece_cnt_){
                  qInfo() << "peer sent invalid have index";
                  socket->disconnectFromHost();
                  return;
         }

         if(socket->peer_bitfield().isEmpty()){ // not receiving bitfield impllies peer doesn't have any piece

                  if(socket->fast_ext_enabled()){ // if fast extension was enabled then peer has to send 'Have-None' message first
                           qInfo() << "Peer didn't send any bitfield related message with fast ext";
                           socket->disconnectFromHost();
                           return;
                  }

                  socket->set_peer_bitfield(QBitArray(bitfield_.size(),false));
         }

         socket->peer_bitfield()[peer_have_piece_idx] = true;

         if(!bitfield_[peer_have_piece_idx]){

                  if(socket->peer_choked()){

                           if(!socket->am_interested()){
                                    socket->set_am_interested(true);
                                    socket->send_packet(interested_msg.data());
                           }

                           socket->add_pending_piece(peer_have_piece_idx);
                  }else{
                           if(peer_have_piece_idx == get_current_target_piece()){
                                    send_block_requests(socket,get_current_target_piece());
                           }
                  }
         }
}

void Peer_wire_client::on_bitfield_received(Tcp_socket * const socket,const QByteArray & response,const std::uint32_t payload_size) noexcept {
         constexpr auto msg_begin_offset = 1;
         socket->set_peer_bitfield(util::conversion::convert_to_bits(response.sliced(msg_begin_offset,payload_size)));
         const auto & peer_bitfield = socket->peer_bitfield();

         assert(bitfield_.size() == peer_bitfield.size());

         for(std::ptrdiff_t bit_idx = 0;bit_idx < bitfield_.size();++bit_idx){

                  if(peer_bitfield[bit_idx] && !bitfield_[bit_idx]){

                           if(static_cast<std::uint64_t>(bit_idx) >= total_piece_cnt_){ // spare bits set
                                    qInfo() << "Spare bit was set";
                                    socket->disconnectFromHost();
                                    return;
                           }
                           
                           socket->set_am_interested(true);
                           socket->add_pending_piece(static_cast<std::uint32_t>(bit_idx));
                  }
         }

         if(socket->am_interested()){
                  socket->send_packet(interested_msg.data());
         }
 }

void Peer_wire_client::on_piece_received(Tcp_socket * const socket,const QByteArray & response) noexcept {
         constexpr auto msg_begin_offset = 1;
         const auto received_piece_idx = util::extract_integer<std::uint32_t>(response,msg_begin_offset);

         const auto received_block_idx = [&response = response]{
                  constexpr auto piece_begin_offset = 5;
                  return util::extract_integer<std::uint32_t>(response,piece_begin_offset);
         }();

         const auto received_content = [&response = response]{
                  constexpr auto piece_content_offset = 9;
                  return response.sliced(piece_content_offset);
         }();

         const auto [piece_size,block_size,block_cnt] = get_piece_info(received_piece_idx,received_block_idx);

         if(received_piece_idx >= total_piece_cnt_ || received_block_idx >= block_cnt){
                  qInfo() << "Invalid piece/block index from peer";
                  return;
         }

         auto & [requested_blocks,received_blocks,piece,received_block_cnt] = pieces_[received_piece_idx];

         //todo consider when the peer sends unrequested packet

         if(piece.isEmpty()){
                  piece.resize(static_cast<std::ptrdiff_t>(piece_size));
         }

         if(received_blocks.empty()){
                  received_blocks.resize(block_size);
         }

         if(received_blocks[received_block_idx]){
                  return;
         }

         received_blocks[received_block_idx] = true;
         ++received_block_cnt;

         qInfo() << "receved piece" << received_piece_idx << received_block_idx;

         std::memcpy(&piece[received_block_idx],received_content.data(),static_cast<std::size_t>(received_content.size()));

         if(received_block_cnt == block_cnt){

                  qInfo() << "Got the piece";
                  qInfo() << "Got the piece";
                  qInfo() << "Got the piece";
                  qInfo() << "Got the piece";

                  if(verify_hash(received_piece_idx,pieces_[received_piece_idx].piece)){
                           qInfo() << "Successfully downloaded a piece";

                           bitfield_[received_piece_idx] = true;
                           assert(remaining_pieces_.contains(received_piece_idx));
                           remaining_pieces_.remove(received_piece_idx);

                           emit piece_downloaded(received_piece_idx);
                  }else{
                           qInfo() << "hash verification failed";
                           piece.clear();
                           received_blocks.clear();
                           requested_blocks.clear();
                           received_block_cnt = 0;
                  }
         }else{
                  send_block_requests(socket,received_piece_idx);
         }
}

void Peer_wire_client::on_allowed_fast_received(Tcp_socket * const socket,const std::uint32_t allowed_piece_idx) noexcept {
         assert(socket->fast_ext_enabled());

         if(allowed_piece_idx >= total_piece_cnt_){
                  qInfo() << "invalid allowed fast index";
                  socket->disconnectFromHost();
         }else if(socket->peer_bitfield()[allowed_piece_idx]){
                  qInfo() << "Sending fast request for" << allowed_piece_idx;
                  send_block_requests(socket,allowed_piece_idx);
         }else{
                  qInfo() << "Peer doesn't have the fast packet";
         }
}

void Peer_wire_client::communicate_with_peer(Tcp_socket * const socket){
         assert(socket->handshake_done());
         
         const auto response_opt = socket->receive_packet();

         if(!response_opt){
                  return;
         }

         const auto & [response_size,response] = *response_opt;

         assert(response_size && response_size == response.size());
         
         const auto received_msg_id = [&response = response]{
                  constexpr auto msg_id_offset = 0;
                  return static_cast<Message_Id>(util::extract_integer<std::uint8_t>(response,msg_id_offset));
         }();

         [[maybe_unused]] auto extract_piece_metadata = [&response = response]{

                  assert(response.size() > 12);

                  const auto piece_index = [&response = response]{
                           constexpr auto msg_begin_offset = 1;
                           return util::extract_integer<std::uint32_t>(response,msg_begin_offset);
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

         if(!valid_response(socket,response,received_msg_id)){
                  qInfo() << "Invalid peer response" << response;
                  socket->disconnectFromHost();
                  return;
         }

         qInfo() << received_msg_id;
         constexpr auto msg_offset = 1;

         switch(received_msg_id){
                  case Message_Id::Choke : {
                           socket->set_peer_choked(true);
                           emit socket->got_choked();
                           break;
                  }

                  case Message_Id::Unchoke : {
                           socket->set_peer_choked(false);
                           break;
                  }

                  case Message_Id::Interested : {
                           socket->set_peer_interested(true);
                           socket->send_packet(unchoke_msg.data());
                           break;
                  }

                  case Message_Id::Uninterested : {
                           socket->set_peer_interested(false);
                           break;
                  }

                  case Message_Id::Have : {
                           on_have_message_received(socket,util::extract_integer<std::uint32_t>(response,msg_offset));
                           break;
                  }

                  case Message_Id::Bitfield : {
                           on_bitfield_received(socket,response,response_size - 1);
                           break;
                  }

                  case Message_Id::Request : {
                           // const auto [requested_piece_idx,requested_piece_offset,requested_piece_size] = extract_piece_metadata();
                           break;
                  }

                  case Message_Id::Piece : {
                           on_piece_received(socket,response);
                           break;
                  }

                  case Message_Id::Cancel : {
                           // const auto [cancelled_piece_idx,cancelled_piece_offset,cancelled_piece_size] = extract_piece_metadata();
                           break;
                  }

                  case Message_Id::Have_All : {
                           assert(util::conversion::convert_to_hex(static_cast<quint32_be>(1)) + response.toHex() == have_all_msg.data());
                           socket->set_peer_bitfield(QBitArray(bitfield_.size(),true));

                           auto & peer_bitfield = socket->peer_bitfield();

                           for(std::ptrdiff_t idx = bitfield_.size() - 1,bits = static_cast<std::ptrdiff_t>(spare_bitfield_bits_);bits;--idx,--bits){
                                    assert(idx >= 0);
                                    peer_bitfield[idx] = false;
                           }

                           break;
                  }

                  case Message_Id::Have_None : {
                           assert(util::conversion::convert_to_hex(static_cast<quint32_be>(1)) + response.toHex() == have_none_msg.data());
                           socket->set_peer_bitfield(QBitArray(bitfield_.size(),false));
                           break;
                  }

                  case Message_Id::Reject_Request : {
                           emit socket->request_rejected();
                           break;
                  }

                  case Message_Id::Allowed_Fast : {
                           on_allowed_fast_received(socket,util::extract_integer<std::uint32_t>(response,msg_offset));
                           break;
                  }

                  default : {
                           qInfo() << "Invalid message id" << received_msg_id;
                  }
         }
}