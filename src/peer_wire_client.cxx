#include "peer_wire_client.hxx"
#include "tcp_socket.hxx"
#include "download_tracker.hxx"

#include <QPointer>
#include <QFile>

Peer_wire_client::Peer_wire_client(bencode::Metadata & torrent_metadata,util::Download_resources resources,QByteArray id,QByteArray info_sha1_hash)
         : torrent_metadata_(torrent_metadata)
         , torrent_file_sizes_(static_cast<qsizetype>(torrent_metadata_.file_info.size()))
         , file_handles_(std::move(resources.file_handles))
         , id_(std::move(id))
         , info_sha1_hash_(std::move(info_sha1_hash))
         , handshake_msg_(craft_handshake_message())
         , total_byte_cnt_(torrent_metadata_.single_file ? torrent_metadata_.single_file_size : torrent_metadata_.multiple_files_size)
         , piece_size_(torrent_metadata_.piece_length)
         , total_piece_cnt_(static_cast<std::int32_t>(std::ceil(static_cast<double>(total_byte_cnt_) / static_cast<double>(piece_size_))))
         , spare_bit_cnt_(total_piece_cnt_ % 8 ? 8 - total_piece_cnt_ % 8 : 0)
         , average_block_cnt_(static_cast<std::int32_t>(std::ceil(static_cast<double>(piece_size_) / max_block_size)))
         , bitfield_(static_cast<qsizetype>(total_piece_cnt_ + spare_bit_cnt_))
         , pieces_(total_piece_cnt_)
         , tracker_(resources.tracker)
{
         assert(piece_size_);
         assert(tracker_);
         assert(!info_sha1_hash_.isEmpty());
         assert(!id_.isEmpty());
         assert(file_handles_.size() == static_cast<qsizetype>(torrent_metadata_.file_info.size()));
         assert(file_handles_.size() == torrent_file_sizes_.size());

         for(qsizetype idx = 0;idx < torrent_file_sizes_.size();++idx){
                  torrent_file_sizes_[idx] = torrent_metadata_.file_info[static_cast<std::size_t>(idx)].second;
                  assert(torrent_file_sizes_[idx]);
         }

         rem_pieces_.reserve(total_piece_cnt_);

         for(std::int32_t piece_idx = 0;piece_idx < total_piece_cnt_;++piece_idx){

                  if(!bitfield_[piece_idx]){
                           rem_pieces_.insert(piece_idx);
                  }
         }

         connect(this,&Peer_wire_client::piece_downloaded,[this](const std::int32_t dled_piece_idx){
                  ++dled_piece_cnt_;
                  assert(dled_piece_cnt_ <= total_piece_cnt_);

                  dled_byte_cnt_ += get_piece_size(dled_piece_idx);
                  assert(dled_byte_cnt_ <= total_byte_cnt_);

                  assert(!bitfield_[dled_piece_idx]);
                  bitfield_[dled_piece_idx] = true;

                  tracker_->download_progress_update(dled_piece_cnt_,total_piece_cnt_);

                  assert(rem_pieces_.contains(dled_piece_idx));
                  rem_pieces_.remove(dled_piece_idx);
         });

         verify_existing_pieces();
}

[[nodiscard]]
bool Peer_wire_client::verify_piece_hash(const QByteArray & received_piece,const std::int32_t piece_idx) const noexcept {
         assert(piece_idx < total_piece_cnt_);

         constexpr auto sha1_hash_byte_cnt = 20;

         assert(torrent_metadata_.pieces.size() % sha1_hash_byte_cnt == 0);
         assert(piece_idx * sha1_hash_byte_cnt < static_cast<qsizetype>(torrent_metadata_.pieces.size()));

         const auto beg_hash_idx = piece_idx * sha1_hash_byte_cnt;
         const QByteArray piece_hash(torrent_metadata_.pieces.substr(static_cast<std::size_t>(beg_hash_idx),sha1_hash_byte_cnt).data(),sha1_hash_byte_cnt);

         return piece_hash == QCryptographicHash::hash(received_piece,QCryptographicHash::Algorithm::Sha1);
}

[[nodiscard]]
Peer_wire_client::Piece_metadata Peer_wire_client::get_piece_info(const std::int32_t piece_idx,const std::int32_t offset) const noexcept {
         const auto piece_size = get_piece_size(piece_idx);
         assert(piece_size > 0 && piece_size <= std::numeric_limits<std::int32_t>::max());

         const auto total_block_cnt = static_cast<std::int32_t>(std::ceil(static_cast<double>(piece_size) / max_block_size));
         assert(offset / max_block_size < total_block_cnt);

         const auto block_size = offset / max_block_size == total_block_cnt - 1 && piece_size % max_block_size ? piece_size % max_block_size : max_block_size;
         assert(block_size > 0 && block_size < std::numeric_limits<std::int32_t>::max());

         return {static_cast<std::int32_t>(piece_size),static_cast<std::int32_t>(block_size),total_block_cnt};
}

void Peer_wire_client::verify_existing_pieces() noexcept {

         auto verify_piece = [this](auto verify_piece,const std::int32_t piece_idx) -> void {
                  assert(piece_idx >= 0);

                  if(piece_idx < total_piece_cnt_){

                           //! consider storing random pieces
                           if(const auto read_piece = read_from_disk(piece_idx)){
                                    emit piece_downloaded(piece_idx);
                           }

                           QTimer::singleShot(0,this,[verify_piece,piece_idx]{
                                    verify_piece(verify_piece,piece_idx + 1);
                           });
                  }else{
                           tracker_->download_progress_update(dled_byte_cnt_,total_byte_cnt_);
                           emit existing_pieces_verified();
                  }
         };

         dled_piece_cnt_ = 0;
         dled_byte_cnt_ = 0;
         bitfield_.fill(false);

         QTimer::singleShot(0,this,[verify_piece]{
                  verify_piece(verify_piece,0);
         });
}

void Peer_wire_client::connect_to_peers(const QList<QUrl> & peer_urls) noexcept {
         assert(!peer_urls.isEmpty());

         for(const auto & peer_url : peer_urls){

                  if(active_peers_.contains(peer_url)){
                           continue;
                  }

                  active_peers_.insert(peer_url);

                  auto * const socket = new Tcp_socket(peer_url,this);

                  connect(socket,&Tcp_socket::connected,this,[this,socket,peer_url]{
                           assert(!socket->handshake_done);
                           socket->send_packet(handshake_msg_);

                           connect(socket,&Tcp_socket::readyRead,this,[this,socket]{
                                    assert(socket->state() == Tcp_socket::ConnectedState);
                                    on_socket_ready_read(socket);
                           });

                           connect(this,&Peer_wire_client::piece_downloaded,socket,[socket](const std::int32_t dled_piece_idx){

                                    if(socket->state() == Tcp_socket::SocketState::ConnectedState){
                                             socket->send_packet(craft_have_message(dled_piece_idx));
                                    }
                           });      

                           connect(socket,&Tcp_socket::disconnected,this,[this,socket,peer_url]{
                                    
                                    if(socket->handshake_done){
                                             assert(active_connection_cnt_);
                                             --active_connection_cnt_;
                                    }

                                    [[maybe_unused]] const auto remove_success = active_peers_.remove(peer_url);
                                    assert(remove_success);
                           });
                  });
         }
}

void Peer_wire_client::on_socket_ready_read(Tcp_socket * const socket) noexcept {

         auto recall_if_unread = [this,socket]{

                  if(socket->bytesAvailable()){

                           QTimer::singleShot(0,this,[this,socket = QPointer(socket)]{
                                    
                                    if(socket){
                                             on_socket_ready_read(socket);
                                    }
                           });
                  }
         };
         
         try {
                  if(socket->handshake_done){
                           assert(!socket->peer_id.isEmpty());
                           communicate_with_peer(socket);
                           return recall_if_unread();
                  }

                  if(auto peer_info = verify_handshake_response(socket)){
                           auto & [peer_info_hash,peer_id] = *peer_info;

                           if(info_sha1_hash_ == peer_info_hash){
                                    assert(socket->peer_id.isEmpty());
                                    assert(!socket->handshake_done);

                                    ++active_connection_cnt_;
                                    socket->peer_id = std::move(peer_id);
                                    socket->handshake_done = true;

                                    assert(dled_piece_cnt_ <= total_piece_cnt_);

                                    if(dled_piece_cnt_ == total_piece_cnt_ && socket->fast_ext_enabled){
                                             assert(rem_pieces_.isEmpty());
                                             socket->send_packet(have_all_msg.data());
                                    }else if(!dled_piece_cnt_){

                                             if(socket->fast_ext_enabled){
                                                      // todo: add again
                                                      assert(rem_pieces_.size() == total_piece_cnt_);
                                                      socket->send_packet(have_none_msg.data());
                                             }

                                    }else{
                                             assert(rem_pieces_.size() < total_piece_cnt_);
                                             socket->send_packet(craft_bitfield_message(bitfield_));
                                    }

                                    return recall_if_unread();
                           }
                  }

                  qDebug() << "Invalid handshake/info_hash doens't match";
                  socket->abort();
                  
         }catch(const std::exception & exception){
                  qDebug() << exception.what();
                  socket->abort();
         }
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_request_message(const std::int32_t piece_idx,const std::int32_t offset) const noexcept {
         using util::conversion::convert_to_hex;

         auto request_msg = []{
                  constexpr qint32_be request_msg_size(13);
                  return convert_to_hex(request_msg_size);
         }();

         request_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Request));
         request_msg += convert_to_hex(static_cast<qint32_be>(piece_idx));
         request_msg += convert_to_hex(static_cast<qint32_be>(offset));
         request_msg += convert_to_hex(static_cast<qint32_be>(get_piece_info(piece_idx,offset).block_size));

         assert(request_msg.size() == 34);
         return request_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_cancel_message(const std::int32_t piece_idx,const std::int32_t offset) const noexcept {
         // todo: exactly the same as request message except the id
         using util::conversion::convert_to_hex;

         auto cancel_msg = []{
                  constexpr qint32_be cancel_msg_size(13);
                  return convert_to_hex(cancel_msg_size);
         }();

         cancel_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Cancel));
         cancel_msg += convert_to_hex(static_cast<qint32_be>(piece_idx));
         cancel_msg += convert_to_hex(static_cast<qint32_be>(offset));
         cancel_msg += convert_to_hex(get_piece_info(piece_idx,offset).block_size);

         assert(cancel_msg.size() == 34);
         return cancel_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_allowed_fast_message(const std::int32_t piece_idx) noexcept {
         using util::conversion::convert_to_hex;

         auto allowed_fast_msg = []{
                  constexpr qint32_be allowed_fast_msg_size(5);
                  return convert_to_hex(allowed_fast_msg_size);
         }();

         allowed_fast_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Allowed_Fast));
         allowed_fast_msg += convert_to_hex(static_cast<qint32_be>(piece_idx));

         assert(allowed_fast_msg.size() == 18);
         return allowed_fast_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_have_message(const std::int32_t piece_idx) noexcept {
         using util::conversion::convert_to_hex;

         auto have_msg = []{
                  constexpr qint32_be have_msg_size(5);
                  return convert_to_hex(have_msg_size);
         }();

         have_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Have));
         have_msg += convert_to_hex(static_cast<qint32_be>(piece_idx));
         
         assert(have_msg.size() == 18);
         return have_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_handshake_message() const noexcept {

         auto handshake_msg = []{
                  constexpr std::int8_t pstrlen = 19;
                  constexpr std::string_view protocol("BitTorrent protocol");
                  static_assert(protocol.size() == pstrlen);

                  return util::conversion::convert_to_hex(pstrlen) + QByteArray(protocol.data(),protocol.size()).toHex();
         }();

         handshake_msg += reserved_bytes + info_sha1_hash_ + id_;

         assert(handshake_msg.size() == 136);
         return handshake_msg;
}

QByteArray Peer_wire_client::craft_reject_message(const std::int32_t piece_idx,const std::int32_t piece_offset,const std::int32_t byte_cnt) noexcept {
         using util::conversion::convert_to_hex;

         auto reject_message = []{
                  constexpr qint32_be reject_msg_size(13);
                  return convert_to_hex(reject_msg_size);
         }();

         reject_message += convert_to_hex(static_cast<std::int8_t>(Message_Id::Reject_Request));
         reject_message += convert_to_hex(static_cast<qint32_be>(piece_idx));
         reject_message += convert_to_hex(static_cast<qint32_be>(piece_offset));
         reject_message += convert_to_hex(static_cast<qint32_be>(byte_cnt));

         assert(reject_message.size() == 26);
         return reject_message;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_piece_message(const QByteArray & piece_data,const std::int32_t piece_idx,const std::int32_t offset) noexcept {
         using util::conversion::convert_to_hex;

         auto piece_msg = [piece_size = piece_data.size()]{
                  const qint32_be piece_msg_size(9 + static_cast<std::int32_t>(piece_size));
                  return convert_to_hex(piece_msg_size);
         }();
         
         piece_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Piece));
         piece_msg += convert_to_hex(static_cast<qint32_be>(piece_idx));
         piece_msg += convert_to_hex(static_cast<qint32_be>(offset));
         assert(piece_msg.size() == 26);
         piece_msg += piece_data.toHex();
         
         return piece_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_bitfield_message(const QBitArray & bitfield) noexcept {

         auto bitfield_msg = [&bitfield]{
                  assert(bitfield.size() % 8 == 0);
                  const qint32_be bitfield_msg_size(1 + static_cast<std::int32_t>(bitfield.size()) / 8);
                  return util::conversion::convert_to_hex(bitfield_msg_size);
         }();

         bitfield_msg += util::conversion::convert_to_hex(static_cast<std::int8_t>(Message_Id::Bitfield));

         assert(bitfield_msg.size() == 10);
         assert(util::conversion::convert_to_bits(QByteArray::fromHex(util::conversion::convert_to_hex_bytes(bitfield))) == bitfield);

         bitfield_msg += util::conversion::convert_to_hex_bytes(bitfield);

         return bitfield_msg;
}

[[nodiscard]]
std::optional<std::pair<QByteArray,QByteArray>> Peer_wire_client::verify_handshake_response(Tcp_socket * const socket){
         constexpr auto expected_response_size = 68;
         const auto response = socket->read(expected_response_size);

         if(response.size() != expected_response_size){
                  qDebug() << "Invalid peer handshake response size";
                  return {};
         }

         {
                  const auto protocol_label_len = [&response]{
                           constexpr auto protocol_label_len_offset = 0;
                           return util::extract_integer<std::int8_t>(response,protocol_label_len_offset);
                  }();

                  if(constexpr auto expected_protocol_label_len = 19;protocol_label_len != expected_protocol_label_len){
                           return {};
                  }

                  const auto protocol_label = [&response,protocol_label_len]{
                           constexpr auto protocol_label_offset = 1;
                           return response.sliced(protocol_label_offset,protocol_label_len);
                  }();

                  if(constexpr std::string_view expected_protocol("BitTorrent protocol");protocol_label.data() != expected_protocol){
                           qDebug() << "Peer is using some woodoo protocol";
                           return {};
                  }
         }

         {
                  const auto peer_reserved_bytes = [&response]{
                           constexpr auto reserved_bytes_offset = 20;
                           constexpr auto reserved_byte_cnt = 8;
                           return response.sliced(reserved_bytes_offset,reserved_byte_cnt);
                  }();

                  const auto peer_reserved_bits = util::conversion::convert_to_bits(peer_reserved_bytes);
                  assert(peer_reserved_bytes.size() * 8 == peer_reserved_bits.size());

                  constexpr auto fast_ext_bit_idx = 61;

                  if(peer_reserved_bits[fast_ext_bit_idx]){
                           socket->fast_ext_enabled = true;
                  }
         }

         auto peer_info_hash = [&response]{
                  constexpr auto sha1_hash_offset = 28;
                  constexpr auto sha1_hash_size = 20;
                  return response.sliced(sha1_hash_offset,sha1_hash_size).toHex();
         }();

         auto peer_id = [&response]{
                  constexpr auto peer_id_offset = 48;
                  constexpr auto peer_id_size = 20;
                  return response.sliced(peer_id_offset,peer_id_size).toHex();
         }();

         return std::make_pair(std::move(peer_info_hash),std::move(peer_id));
}

void Peer_wire_client::send_block_requests(Tcp_socket * const socket,const std::int32_t piece_idx) noexcept {
         assert(piece_idx < total_piece_cnt_);
         assert(!socket->peer_bitfield.isEmpty());
         assert(socket->peer_bitfield[piece_idx]);

         const auto total_block_cnt = get_piece_info(piece_idx,0).total_block_cnt;
         auto & [requested_blocks,received_blocks,piece,received_block_cnt] = pieces_[piece_idx];

         if(requested_blocks.empty()){
                  requested_blocks.resize(total_block_cnt);
         }

         if(received_blocks.isEmpty()){
                  received_blocks.resize(total_block_cnt);
         }

         constexpr auto max_requests = 150;
         constexpr auto max_duplicate_requests = 1;

         for(std::int32_t block_idx = 0,request_sent_cnt = 0;request_sent_cnt < max_requests && block_idx < total_block_cnt;++block_idx){
                  assert(requested_blocks[block_idx] <= max_duplicate_requests);
                  assert(!requested_blocks.empty());

                  if(received_blocks[block_idx] || requested_blocks[block_idx] == max_duplicate_requests){
                           continue;
                  }

                  ++request_sent_cnt;
                  ++requested_blocks[block_idx];

                  socket->send_packet(craft_request_message(piece_idx,block_idx * max_block_size));

                  connect(socket,&Tcp_socket::got_choked,this,[&requested_blocks = requested_blocks,socket,block_idx]{

                           if(!socket->fast_ext_enabled && !requested_blocks.empty()){
                                    assert(requested_blocks[block_idx]);
                                    --requested_blocks[block_idx];
                           }
                  });

                  const auto dec_connection = connect(socket,&Tcp_socket::disconnected,this,[&requested_blocks = requested_blocks,socket,block_idx]{

                           if(!socket->peer_choked && !requested_blocks.empty()){
                                    assert(requested_blocks[block_idx]);
                                    --requested_blocks[block_idx];
                           }
                  });

                  assert(dec_connection);
                  // todo: figure
                  // assert(disconnect(dec_connection));

                  // connect(socket,&Tcp_socket::request_rejected,this,[&requested_blocks = requested_blocks,socket,block_idx,dec_connection]{
                  //          assert(socket->fast_ext_enabled());

                  //          {
                  //                   [[maybe_unused]] const auto disconnected = disconnect(dec_connection);
                  //                   assert(disconnected);
                  //          }

                  //          if(!requested_blocks.empty()){
                  //                   assert(requested_blocks[block_idx]);
                  //                   --requested_blocks[block_idx];
                  //          }
                  // });
         }
}

void Peer_wire_client::on_piece_request_received(Tcp_socket * const socket,const QByteArray & response) noexcept {
         const auto [piece_idx,offset,byte_cnt] = extract_piece_metadata(response);

         auto send_reject_message = [socket,piece_idx = piece_idx,offset = offset,byte_cnt = byte_cnt]{

                  if(socket->fast_ext_enabled){
                           socket->send_packet(craft_reject_message(piece_idx,offset,byte_cnt));
                  }
         };

         assert(!bitfield_.isEmpty());

         if(piece_idx >= total_piece_cnt_ || !bitfield_[piece_idx]){
                  return send_reject_message();
         }

         auto handle_piece_request = [socket,send_reject_message,piece_idx = piece_idx,offset = offset,byte_cnt = byte_cnt](const QByteArray & piece){

                  if(offset + byte_cnt <= piece.size()){
                           qDebug() << "Sending the poiece";
                           socket->send_packet(craft_piece_message(piece.sliced(offset,byte_cnt),piece_idx,offset));
                  }else{
                           send_reject_message();
                  }
         };

         if(const auto & buffered_piece = pieces_[piece_idx].data;buffered_piece.isEmpty()){

                  if(const auto disk_piece = read_from_disk(piece_idx)){
                           assert(verify_piece_hash(*disk_piece,piece_idx));
                           handle_piece_request(*disk_piece);
                  }else{
                           send_reject_message();
                  }
         }else{
                  assert(verify_piece_hash(buffered_piece,piece_idx));
                  handle_piece_request(buffered_piece);
         }
}

[[nodiscard]]
std::optional<std::pair<qsizetype,qsizetype>> Peer_wire_client::get_beginning_file_info(const std::int32_t piece_idx) const noexcept {
         assert(piece_idx >= 0 && piece_idx < total_piece_cnt_);

         for(qsizetype file_handle_idx = 0,file_size_sum = 0;file_handle_idx < file_handles_.size();++file_handle_idx){
                  const auto file_size_sum_old = file_size_sum;

                  file_size_sum += torrent_file_sizes_[file_handle_idx];
                  assert(file_size_sum > file_size_sum_old);

                  const auto ending_piece_idx = static_cast<std::int32_t>(std::ceil(static_cast<double>(file_size_sum) / static_cast<double>(piece_size_))) - 1;

                  if(ending_piece_idx >= piece_idx){
                           const auto prev_offset_delta = file_size_sum_old % piece_size_ ? piece_size_ - file_size_sum_old % piece_size_ : 0;
                           assert((prev_offset_delta + file_size_sum_old) % piece_size_ == 0);

                           const auto beg_piece_idx = file_size_sum_old / piece_size_ + static_cast<bool>(prev_offset_delta);
                           assert(piece_idx >= beg_piece_idx);
                           
                           const auto prior_piece_cnt = piece_idx - beg_piece_idx;
                           assert(prior_piece_cnt >= 0);
                           const auto file_offset = prior_piece_cnt * piece_size_ + prev_offset_delta;

                           assert(file_offset >= 0 && file_offset < torrent_file_sizes_[file_handle_idx]);

                           return std::make_pair(file_handle_idx,file_offset);
                  }
         }

         return {};
}

[[nodiscard]]
bool Peer_wire_client::write_to_disk(const QByteArray & received_piece,const std::int32_t received_piece_idx) noexcept {
         assert(received_piece.size() == get_piece_size(received_piece_idx));
         assert(verify_piece_hash(received_piece,received_piece_idx));

         const auto file_handle_info = get_beginning_file_info(received_piece_idx);

         if(!file_handle_info){ // todo: report to tracker and stop the download
                  return false;
         }
         
         const auto [beg_file_handle_idx,beg_file_offset] = *file_handle_info;

         assert(beg_file_handle_idx < file_handles_.size());
         assert(beg_file_offset >= 0 && beg_file_offset < torrent_file_sizes_[beg_file_handle_idx]);

         const auto beg_file_byte_cnt = torrent_file_sizes_[beg_file_handle_idx] - beg_file_offset;

         for(std::int64_t written_byte_cnt = 0,file_handle_idx = beg_file_handle_idx;written_byte_cnt < received_piece.size();++file_handle_idx){
                  assert(file_handle_idx < file_handles_.size());

                  auto * const file_handle = file_handles_[file_handle_idx];
                  file_handle->seek(written_byte_cnt ? 0 : beg_file_offset);

                  const auto to_write_byte_cnt = std::min(received_piece.size() - written_byte_cnt,written_byte_cnt ? file_handle->size() : beg_file_byte_cnt);
                  assert(to_write_byte_cnt);

                  assert(file_handle->pos() + to_write_byte_cnt <= torrent_file_sizes_[file_handle_idx]);
                  const auto newly_written_byte_cnt = file_handle->write(received_piece.sliced(written_byte_cnt,to_write_byte_cnt));
                  assert(file_handle->pos() <= torrent_file_sizes_[file_handle_idx]);

                  if(newly_written_byte_cnt != to_write_byte_cnt){
                           qDebug() << "Could not write to file";
                           return false;
                  }

                  written_byte_cnt += newly_written_byte_cnt;
                  assert(written_byte_cnt <= received_piece.size());
         }

         assert(received_piece == read_from_disk(received_piece_idx));

         return true;
}

[[nodiscard]]
std::optional<QByteArray> Peer_wire_client::read_from_disk(const std::int32_t requested_piece_idx) noexcept {
         assert(requested_piece_idx >= 0 && requested_piece_idx < total_piece_cnt_);

         const auto beg_file_handle_info = get_beginning_file_info(requested_piece_idx);

         if(!beg_file_handle_info){
                  return {};
         }

         const auto requested_piece_size = get_piece_size(requested_piece_idx);

         QByteArray resultant_piece;
         resultant_piece.reserve(requested_piece_size);

         const auto [beg_file_handle_idx,beg_file_offset] = *beg_file_handle_info;
         assert(beg_file_handle_idx < file_handles_.size());

         for(qsizetype file_handle_idx = beg_file_handle_idx;resultant_piece.size() < requested_piece_size;++file_handle_idx){

                  if(file_handle_idx == file_handles_.size()){
                           return {};
                  }

                  auto * const file_handle = file_handles_[file_handle_idx];
                  file_handle->seek(resultant_piece.size() ? 0 : beg_file_offset);

                  const auto to_read_byte_cnt = requested_piece_size - resultant_piece.size();
                  assert(to_read_byte_cnt);
                  const auto newly_read_bytes = file_handle->read(to_read_byte_cnt);

                  if(newly_read_bytes.size() != to_read_byte_cnt){
                           return {};
                  }
                  
                  resultant_piece += newly_read_bytes;
         }

         if(!verify_piece_hash(resultant_piece,requested_piece_idx)){
                  return {};
         }

         return resultant_piece;
}

[[nodiscard]]
bool Peer_wire_client::is_valid_response(Tcp_socket * const socket,const QByteArray & response,const Message_Id received_msg_id) noexcept {
         constexpr auto max_msg_id = 17;

         if(static_cast<std::int32_t>(received_msg_id) > max_msg_id){
                  qDebug() << "peer sent unexpected id" << received_msg_id;
                  return false;
         }

         constexpr auto pseudo = 0;

         constexpr std::array<std::int32_t,max_msg_id + 1> expected_response_sizes {
                  1, // choke
                  1, // unchoke
                  1, // interested
                  1, // uninterested
                  5, // have
                  pseudo,
                  9, // request
                  pseudo,
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

                           if(received_msg_idx >= static_cast<std::int32_t>(Message_Id::Suggest_Piece) && !socket->fast_ext_enabled){
                                    qDebug() << "peer sent fast ext ids without enabling ext";
                                    return false;
                           }

                           return response.size() == expected_size;
                  }
         }

         __builtin_unreachable();
}

void Peer_wire_client::on_unchoke_message_received(Tcp_socket * const socket) noexcept {
         socket->peer_choked = false;

         if(socket->pending_pieces.contains(get_target_piece_idx())){
                  send_block_requests(socket,get_target_piece_idx());
         }
}

void Peer_wire_client::on_have_message_received(Tcp_socket * const socket,const std::int32_t peer_have_piece_idx) noexcept {

         if(peer_have_piece_idx >= total_piece_cnt_){
                  qDebug() << "peer sent invalid have index";
                  return socket->abort();
         }

         if(socket->peer_bitfield.isEmpty()){

                  if(socket->fast_ext_enabled){
                           qDebug() << "Peer didn't send any bitfield related message with fast ext";
                           return socket->abort();
                  }

                  socket->peer_bitfield = QBitArray(bitfield_.size(),false);
         }

         socket->peer_bitfield[peer_have_piece_idx] = true;

         if(!bitfield_[peer_have_piece_idx]){

                  if(socket->peer_choked){

                           if(!socket->am_interested){
                                    socket->am_interested = true;
                                    socket->send_packet(interested_msg.data());
                           }

                           socket->pending_pieces.insert(peer_have_piece_idx);
                  }else if(peer_have_piece_idx == get_target_piece_idx()){
                           send_block_requests(socket,get_target_piece_idx());
                  }else if(socket->fast_ext_enabled){
                           emit socket->fast_have_msg_received(peer_have_piece_idx);
                  }
         }
}

void Peer_wire_client::on_bitfield_received(Tcp_socket * const socket,const QByteArray & response,const std::int32_t payload_size) noexcept {
         constexpr auto msg_begin_offset = 1;
         socket->peer_bitfield = util::conversion::convert_to_bits(response.sliced(msg_begin_offset,payload_size));

         assert(bitfield_.size() == socket->peer_bitfield.size());

         for(qsizetype bit_idx = 0;bit_idx < bitfield_.size();++bit_idx){

                  if(bit_idx >= total_piece_cnt_ && socket->peer_bitfield[bit_idx]){
                           qDebug() << "Spare bit was set";
                           return socket->abort();
                  }

                  if(socket->peer_bitfield[bit_idx] && !bitfield_[bit_idx]){
                           socket->am_interested = true;
                           socket->pending_pieces.insert(static_cast<std::int32_t>(bit_idx));
                  }
         }

         if(socket->am_interested){
                  socket->send_packet(interested_msg.data());
         }
}

void Peer_wire_client::on_piece_downloaded(Piece & dled_piece,const std::int32_t dled_piece_idx) noexcept {

         auto release_piece_memory = [&dled_piece,dled_piece_idx]{
                  qDebug() << "releasing piece memory" << dled_piece_idx;

                  auto & [requested_blocks,received_blocks,piece_data,received_block_cnt] = dled_piece;

                  assert(!piece_data.isEmpty());
                  assert(!received_blocks.isEmpty());
                  assert(!requested_blocks.isEmpty());

                  piece_data.clear();
                  requested_blocks.clear();
                  received_blocks.clear();

                  piece_data.shrink_to_fit();
                  requested_blocks.shrink_to_fit();
         };

         if(verify_piece_hash(dled_piece.data,dled_piece_idx) && write_to_disk(dled_piece.data,dled_piece_idx)){
                  qDebug() << "piece successfully downloaded" << dled_piece_idx;
                  
                  emit piece_downloaded(dled_piece_idx);

                  constexpr std::chrono::seconds memory_release_timeout(30);
                  QTimer::singleShot(memory_release_timeout,this,release_piece_memory);
         }else{
                  qDebug() << "hash verification failed";
                  release_piece_memory();
         }
}

void Peer_wire_client::on_piece_received(Tcp_socket * const socket,const QByteArray & response) noexcept {

         const auto received_piece_idx = [&response]{
                  constexpr auto msg_begin_offset = 1;
                  return util::extract_integer<std::int32_t>(response,msg_begin_offset);
         }();

         const auto received_offset = [&response = response]{
                  constexpr auto piece_begin_offset = 5;
                  return util::extract_integer<std::int32_t>(response,piece_begin_offset);
         }();

         const auto newly_received_block = [&response = response]{
                  constexpr auto piece_content_offset = 9;
                  return response.sliced(piece_content_offset);
         }();

         if(received_piece_idx >= total_piece_cnt_){
                  qDebug() << "Invalid piece idx from peer";
                  return;
         }

         const auto received_block_idx = received_offset / max_block_size;

         const auto [piece_size,block_size,total_block_cnt] = get_piece_info(received_piece_idx,received_block_idx);

         if(received_block_idx >= total_block_cnt){
                  qDebug() << "Invalid piece/block index from peer";
                  return;
         }

         assert(received_piece_idx < pieces_.size());
         auto & [requested_blocks,received_blocks,piece_data,received_block_cnt] = pieces_[received_piece_idx];

         // todo: consider when the peer sends unrequested packet

         if(piece_data.isEmpty()){
                  piece_data.resize(static_cast<qsizetype>(piece_size));
         }

         if(received_blocks.isEmpty()){
                  received_blocks.resize(total_block_cnt);
         }

         assert(received_block_idx < total_block_cnt);

         if(received_blocks[received_block_idx]){
                  qDebug() << "Aready recieved" << received_piece_idx << received_offset;
                  return;
         }

         received_blocks[received_block_idx] = true;
         ++received_block_cnt;

         qDebug() << "new piece received" << received_piece_idx << received_offset;

         assert(received_blocks.size() == total_block_cnt);
         
         for(qsizetype idx = 0;idx < newly_received_block.size();++idx){
                  assert(received_offset + idx < piece_data.size());
                  piece_data[received_offset + idx] = newly_received_block[idx];
         }

         if(received_block_cnt == total_block_cnt){
                  assert(received_piece_idx <= pieces_.size());
                  on_piece_downloaded(pieces_[received_piece_idx],received_piece_idx);
         }else{
                  send_block_requests(socket,received_piece_idx);
         }
}

void Peer_wire_client::on_allowed_fast_received(Tcp_socket * const socket,const std::int32_t allowed_piece_idx) noexcept {
         assert(socket->fast_ext_enabled);
         assert(!socket->peer_bitfield.isEmpty());
         
         if(allowed_piece_idx >= total_piece_cnt_){
                  qDebug() << "invalid allowed fast index";
                  socket->abort();
         }else if(socket->peer_bitfield[allowed_piece_idx]){
                  send_block_requests(socket,allowed_piece_idx);
         }else{
                  connect(socket,&Tcp_socket::fast_have_msg_received,this,[this,socket,allowed_piece_idx](const std::int32_t peer_have_piece_idx){

                           if(peer_have_piece_idx == allowed_piece_idx){
                                    qDebug() << "Sending late fast piece request" << peer_have_piece_idx;
                                    send_block_requests(socket,peer_have_piece_idx);
                           }
                  });
         }
}

[[nodiscard]]
std::tuple<std::int32_t,std::int32_t,std::int32_t> Peer_wire_client::extract_piece_metadata(const QByteArray & response){
         assert(response.size() > 12);

         const auto piece_index = [&response = response]{
                  constexpr auto msg_begin_offset = 1;
                  return util::extract_integer<std::int32_t>(response,msg_begin_offset);
         }();

         const auto piece_offset = [&response = response]{
                  constexpr auto piece_begin_offset = 5;
                  return util::extract_integer<std::int32_t>(response,piece_begin_offset);
         }();

         const auto piece_size = [&response = response]{
                  constexpr auto piece_size_offset = 9;
                  return util::extract_integer<std::int32_t>(response,piece_size_offset);
         }();

         return std::make_tuple(piece_index,piece_offset,piece_size);
}

void Peer_wire_client::communicate_with_peer(Tcp_socket * const socket){
         assert(socket->handshake_done);
         assert(!socket->peer_id.isEmpty());
         
         const auto response_opt = socket->receive_packet();

         if(!response_opt){
                  return;
         }

         const auto & [response_size,response] = *response_opt;
         assert(response_size && response_size == response.size());
         
         const auto received_msg_id = [&response = response]{
                  constexpr auto msg_id_offset = 0;
                  return static_cast<Message_Id>(util::extract_integer<std::int8_t>(response,msg_id_offset));
         }();

         if(!is_valid_response(socket,response,received_msg_id)){
                  qDebug() << "Invalid peer response" << response;
                  return socket->abort();
         }

         qDebug() << received_msg_id << active_connection_cnt_ << QByteArray::fromHex(socket->peer_id);

         switch(received_msg_id){

                  case Message_Id::Choke : {
                           socket->peer_choked = true;
                           emit socket->got_choked();
                           break;
                  }

                  case Message_Id::Unchoke : {
                           socket->peer_choked = false;
                           break;
                  }

                  case Message_Id::Interested : {
                           socket->peer_interested = true;
                           // todo: unchoke only if the condtions are met
                           socket->send_packet(unchoke_msg.data());
                           
                           break;
                  }

                  case Message_Id::Uninterested : {
                           socket->peer_interested = false;
                           break;
                  }

                  case Message_Id::Have : {
                           constexpr auto msg_offset = 1;
                           on_have_message_received(socket,util::extract_integer<std::int32_t>(response,msg_offset));
                           break;
                  }

                  case Message_Id::Bitfield : {
                           on_bitfield_received(socket,response,response_size - 1);
                           break;
                  }

                  case Message_Id::Request : {
                           on_piece_request_received(socket,response);
                           break;
                  }

                  case Message_Id::Piece : {
                           on_piece_received(socket,response);
                           break;
                  }

                  case Message_Id::Cancel : {
                           // todo
                           // const auto [cancelled_piece_idx,cancelled_piece_offset,cancelled_piece_size] = extract_piece_metadata();
                           break;
                  }

                  case Message_Id::Have_All : {
                           socket->peer_bitfield = QBitArray(bitfield_.size());

                           assert(bitfield_.size() - spare_bit_cnt_ >= 0);
                           socket->peer_bitfield.fill(false,socket->peer_bitfield.size() - spare_bit_cnt_,socket->peer_bitfield.size());

                           break;
                  }

                  case Message_Id::Have_None : {
                           socket->peer_bitfield = QBitArray(bitfield_.size());
                           break;
                  }

                  case Message_Id::Reject_Request : {
                           emit socket->request_rejected();
                           break;
                  }

                  case Message_Id::Allowed_Fast : {
                           constexpr auto msg_offset = 1;
                           on_allowed_fast_received(socket,util::extract_integer<std::int32_t>(response,msg_offset));
                           break;
                  }

                  default : {
                           qDebug() << "Invalid message id" << received_msg_id;
                           break;
                  }
         }
}