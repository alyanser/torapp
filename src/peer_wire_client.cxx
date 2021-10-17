#include "peer_wire_client.hxx"
#include "tcp_socket.hxx"
#include "download_tracker.hxx"

#include <QCryptographicHash>
#include <QPointer>
#include <QFile>
#include <QMessageBox>

Peer_wire_client::Peer_wire_client(bencode::Metadata & torrent_metadata,util::Download_resources resources,QByteArray id,QByteArray info_sha1_hash)
         : id_(std::move(id))
         , info_sha1_hash_(std::move(info_sha1_hash))
         , handshake_msg_(craft_handshake_message())
         , dl_path_(std::move(resources.dl_path))
         , file_handles_(std::move(resources.file_handles))
         , torrent_metadata_(torrent_metadata)
         , tracker_(resources.tracker)
         , total_byte_cnt_(torrent_metadata.single_file ? torrent_metadata.single_file_size : torrent_metadata.multiple_files_size)
         , piece_size_(torrent_metadata.piece_length)
         , total_piece_cnt_(static_cast<std::int32_t>(std::ceil(static_cast<double>(total_byte_cnt_) / static_cast<double>(piece_size_))))
         , spare_bit_cnt_(total_piece_cnt_ % 8 ? 8 - total_piece_cnt_ % 8 : 0)
         , average_block_cnt_(static_cast<std::int32_t>(std::ceil(static_cast<double>(piece_size_) / max_block_size)))
         , pieces_(total_piece_cnt_)
{
         assert(piece_size_ > 0);
         assert(!info_sha1_hash_.isEmpty());
         assert(!id_.isEmpty());
         assert(file_handles_.size() == static_cast<qsizetype>(torrent_metadata_.file_info.size()));

         for(auto * const file_handle : file_handles_){
                  assert(file_handle->parent());
                  file_handle->setParent(this);
         }
         
         read_settings();
         verify_existing_pieces();

         connect(&settings_timer_,&QTimer::timeout,this,&Peer_wire_client::write_settings);

         connect(this,&Peer_wire_client::existing_pieces_verified,[&setting_timer_ = settings_timer_]{
                  setting_timer_.start(std::chrono::seconds(1));
         });

         connect(this,&Peer_wire_client::piece_verified,[this](const std::int32_t verified_piece_idx){
                  assert(verified_piece_idx >= 0 && verified_piece_idx < total_piece_cnt_);

                  ++dled_piece_cnt_;
                  assert(dled_piece_cnt_ <= total_piece_cnt_);

                  bitfield_[verified_piece_idx] = true;

                  const auto verified_piece_size = get_piece_size(verified_piece_idx);
                  dled_byte_cnt_ += verified_piece_size;
                  assert(dled_byte_cnt_ <= total_byte_cnt_);

                  tracker_->download_progress_update(verified_piece_size,total_byte_cnt_);

                  if(dled_piece_cnt_ == total_piece_cnt_){
                           QMessageBox::information(nullptr,"You did it","You did it you fucking did it");
                  }
         });
         
}

[[nodiscard]]
bool Peer_wire_client::verify_piece_hash(const QByteArray & received_piece,const std::int32_t piece_idx) const noexcept {
         assert(piece_idx >= 0 && piece_idx < total_piece_cnt_);
         constexpr auto sha1_hash_byte_cnt = 20;

         assert(torrent_metadata_.pieces.size() % sha1_hash_byte_cnt == 0);
         assert(static_cast<qsizetype>(piece_idx * sha1_hash_byte_cnt) < static_cast<qsizetype>(torrent_metadata_.pieces.size()));

         const auto beg_hash_idx = piece_idx * sha1_hash_byte_cnt;
         const QByteArray piece_hash(torrent_metadata_.pieces.substr(static_cast<std::size_t>(beg_hash_idx),sha1_hash_byte_cnt).data(),sha1_hash_byte_cnt);

         return piece_hash == QCryptographicHash::hash(received_piece,QCryptographicHash::Algorithm::Sha1);
}

[[nodiscard]]
Peer_wire_client::Piece_metadata Peer_wire_client::get_piece_info(const std::int32_t piece_idx,const std::int32_t offset) const noexcept {
         assert(piece_idx >= 0 && piece_idx < total_piece_cnt_ && offset < piece_size_);
         const auto piece_size = get_piece_size(piece_idx);
         assert(piece_size > 0 && piece_size <= std::numeric_limits<std::int32_t>::max());

         const auto total_block_cnt = static_cast<std::int32_t>(std::ceil(static_cast<double>(piece_size) / max_block_size));
         assert(offset / max_block_size < total_block_cnt);

         const auto block_size = offset / max_block_size == total_block_cnt - 1 && piece_size % max_block_size ? piece_size % max_block_size : max_block_size;
         assert(block_size > 0 && block_size < std::numeric_limits<std::int32_t>::max());

         return {piece_size,block_size,total_block_cnt};
}

void Peer_wire_client::verify_existing_pieces() noexcept {

         auto verify_piece = [this](auto verify_piece_callback,const std::int32_t piece_idx) -> void {
                  assert(piece_idx >= 0 && piece_idx <= total_piece_cnt_);
                  tracker_->verification_progress_update(piece_idx,total_piece_cnt_);

                  if(piece_idx < total_piece_cnt_){

                           if(bitfield_[piece_idx]){
                                    
                                    if(const auto piece = read_from_disk(piece_idx);piece && verify_piece_hash(*piece,piece_idx)){
                                             emit piece_verified(piece_idx);
                                    }
                           }

                           QTimer::singleShot(0,this,[verify_piece_callback,piece_idx]{
                                    verify_piece_callback(verify_piece_callback,piece_idx + 1);
                           });
                  }else{
                           tracker_->set_state(Download_tracker::State::Download);
                           emit existing_pieces_verified();
                  }
         };

         assert(!dled_piece_cnt_);
         assert(!dled_byte_cnt_);

         QTimer::singleShot(0,this,[verify_piece,tracker_ = tracker_]{
                  tracker_->set_state(Download_tracker::State::Verification);
                  verify_piece(verify_piece,0);
         });
}

void Peer_wire_client::on_socket_connected(Tcp_socket * const socket) noexcept {
         socket->send_packet(handshake_msg_);

         connect(socket,&Tcp_socket::readyRead,this,[this,socket]{
                  on_socket_ready_read(socket);
         });

         connect(this,&Peer_wire_client::download_paused,socket,&Tcp_socket::disconnectFromHost);

         connect(this,&Peer_wire_client::piece_verified,socket,[socket](const std::int32_t dled_piece_idx){
                  socket->send_packet(craft_have_message(dled_piece_idx));
         });

         connect(socket,&Tcp_socket::disconnected,this,[this,socket]{
                  
                  if(socket->handshake_done){
                           assert(active_connection_cnt_);
                           --active_connection_cnt_;
                  }

                  [[maybe_unused]] const auto remove_success = active_peers_.remove(socket->peer_url());
                  assert(remove_success);
         });
}

void Peer_wire_client::connect_to_peers(const QList<QUrl> & peer_urls) noexcept {
         assert(!peer_urls.isEmpty());

         for(const auto & peer_url : peer_urls){

                  if(!active_peers_.contains(peer_url)){
                           active_peers_.insert(peer_url);
                           
                           auto * const socket = new Tcp_socket(peer_url,this);

                           connect(socket,&Tcp_socket::connected,this,[this,socket]{
                                    on_socket_connected(socket);
                           });
                  }
         }
}

void Peer_wire_client::on_socket_ready_read(Tcp_socket * const socket) noexcept {

         try {
                  communicate_with_peer(socket);
         }catch(const std::exception & exception){
                  qDebug() << exception.what();
                  return socket->abort();
         }

         if(socket->bytesAvailable()){

                  QTimer::singleShot(0,this,[this,socket = QPointer(socket)]{
                           
                           if(socket){
                                    on_socket_ready_read(socket);
                           }
                  });
         }
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_request_message(const std::int32_t piece_idx,const std::int32_t piece_offset) const noexcept {
         using util::conversion::convert_to_hex;

         auto reqest_msg = []{
                  constexpr auto request_msg_size = 13;
                  return convert_to_hex(request_msg_size);
         }();

         constexpr auto request_msg_size = 34;
         reqest_msg.reserve(request_msg_size);

         reqest_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Request));
         reqest_msg += convert_to_hex(piece_idx);
         reqest_msg += convert_to_hex(piece_offset);
         reqest_msg += convert_to_hex(get_piece_info(piece_idx,piece_offset).block_size);

         assert(reqest_msg.size() == request_msg_size);
         return reqest_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_cancel_message(const std::int32_t piece_idx,const std::int32_t piece_offset) const noexcept {
         // todo: exactly the same as request message except the id
         using util::conversion::convert_to_hex;

         auto cancel_msg = []{
                  constexpr auto cancel_msg_size = 13;
                  return convert_to_hex(cancel_msg_size);
         }();

         constexpr auto cancel_msg_size = 34;
         cancel_msg.reserve(cancel_msg_size);

         cancel_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Cancel));
         cancel_msg += convert_to_hex(piece_idx);
         cancel_msg += convert_to_hex(piece_offset);
         cancel_msg += convert_to_hex(get_piece_info(piece_idx,piece_offset).block_size);

         assert(cancel_msg.size() == cancel_msg_size);
         return cancel_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_allowed_fast_message(const std::int32_t piece_idx) noexcept {
         using util::conversion::convert_to_hex;

         auto allowed_fast_msg = []{
                  constexpr auto allowed_fast_msg_size = 5;
                  return convert_to_hex(allowed_fast_msg_size);
         }();

         constexpr auto allowed_fast_msg_size = 18;
         allowed_fast_msg.reserve(allowed_fast_msg_size);

         allowed_fast_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Allowed_Fast));
         allowed_fast_msg += convert_to_hex(piece_idx);

         assert(allowed_fast_msg.size() == allowed_fast_msg_size);
         return allowed_fast_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_have_message(const std::int32_t piece_idx) noexcept {
         using util::conversion::convert_to_hex;

         auto have_msg = []{
                  constexpr auto have_msg_size = 5;
                  return convert_to_hex(have_msg_size);
         }();

         constexpr auto have_msg_size = 18;
         have_msg.reserve(have_msg_size);

         have_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Have));
         have_msg += convert_to_hex(piece_idx);
         
         assert(have_msg.size() == have_msg_size);
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

         constexpr auto handshake_msg_size = 136;
         handshake_msg.reserve(handshake_msg_size);

         handshake_msg += reserved_bytes.data() + info_sha1_hash_ + id_;

         assert(handshake_msg.size() == handshake_msg_size);
         return handshake_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_reject_message(const std::int32_t piece_idx,const std::int32_t piece_offset,const std::int32_t byte_cnt) noexcept {
         using util::conversion::convert_to_hex;

         auto reject_msg = []{
                  constexpr auto reject_msg_size = 13;
                  return convert_to_hex(reject_msg_size);
         }();
         
         constexpr auto reject_msg_size = 34;
         reject_msg.reserve(reject_msg_size);

         reject_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Reject_Request));
         reject_msg += convert_to_hex(piece_idx);
         reject_msg += convert_to_hex(piece_offset);
         reject_msg += convert_to_hex(byte_cnt);

         assert(reject_msg.size() == reject_msg_size);
         return reject_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_piece_message(const QByteArray & piece_data,const std::int32_t piece_idx,const std::int32_t piece_offset) noexcept {
         using util::conversion::convert_to_hex;

          auto piece_msg = [piece_size = piece_data.size()]{
                  const auto piece_msg_size = 9 + static_cast<std::int32_t>(piece_size);
                  return convert_to_hex(piece_msg_size);
         }();

         const auto piece_msg_size = piece_data.size() * 2 + 26;
         piece_msg.reserve(piece_msg_size);
         
         piece_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Piece));
         piece_msg += convert_to_hex(piece_idx);
         piece_msg += convert_to_hex(piece_offset);
         piece_msg += piece_data.toHex();

         assert(piece_msg.size() == piece_msg_size);
         return piece_msg;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_bitfield_message(const QBitArray & bitfield) noexcept {
         using util::conversion::convert_to_hex;
         using util::conversion::convert_to_bytes;

         assert(bitfield.size() % 8 == 0);

         auto bitfield_msg = [&bitfield]{
                  const auto bitfield_msg_size = 1 + static_cast<std::int32_t>(bitfield.size() / 8);
                  return convert_to_hex(bitfield_msg_size);
         }();

         const auto bitfield_msg_size = bitfield.size() / 8 * 2 + 10;
         bitfield_msg.reserve(bitfield_msg_size);

         bitfield_msg += convert_to_hex(static_cast<std::int8_t>(Message_Id::Bitfield));
         assert(util::conversion::convert_to_bits(QByteArray::fromHex(convert_to_bytes(bitfield))) == bitfield);
         bitfield_msg += convert_to_bytes(bitfield);

         assert(bitfield_msg.size() == bitfield_msg_size);
         return bitfield_msg;
}

[[nodiscard]]
std::optional<std::pair<QByteArray,QByteArray>> Peer_wire_client::verify_handshake_reply(Tcp_socket * const socket,const QByteArray & reply){
         constexpr auto expected_reply_size = 68;

         if(reply.size() != expected_reply_size){
                  qDebug() << "Invalid peer handshake reply size";
                  return {};
         }

         {
                  const auto protocol_label_len = [&reply]{
                           constexpr auto protocol_label_len_offset = 0;
                           return util::extract_integer<std::int8_t>(reply,protocol_label_len_offset);
                  }();

                  if(constexpr auto expected_protocol_label_len = 19;protocol_label_len != expected_protocol_label_len){
                           return {};
                  }

                  const auto protocol_label = [&reply,protocol_label_len]{
                           constexpr auto protocol_label_offset = 1;
                           return reply.sliced(protocol_label_offset,protocol_label_len);
                  }();

                  if(constexpr std::string_view expected_protocol("BitTorrent protocol");protocol_label.data() != expected_protocol){
                           qDebug() << "Peer is using some woodoo protocol";
                           return {};
                  }
         }

         {
                  const auto peer_reserved_bytes = [&reply]{
                           constexpr auto reserved_bytes_offset = 20;
                           constexpr auto reserved_byte_cnt = 8;
                           return reply.sliced(reserved_bytes_offset,reserved_byte_cnt);
                  }();

                  const auto peer_reserved_bits = util::conversion::convert_to_bits(peer_reserved_bytes);
                  assert(peer_reserved_bytes.size() * 8 == peer_reserved_bits.size());

                  if(constexpr auto fast_ext_bit_idx = 61;peer_reserved_bits[fast_ext_bit_idx]){
                           socket->fast_extension_enabled = true;
                  }
         }

         auto peer_info_hash = [&reply]{
                  constexpr auto sha1_hash_offset = 28;
                  constexpr auto sha1_hash_size = 20;
                  return reply.sliced(sha1_hash_offset,sha1_hash_size).toHex();
         }();

         auto peer_id = [&reply]{
                  constexpr auto peer_id_offset = 48;
                  constexpr auto peer_id_size = 20;
                  return reply.sliced(peer_id_offset,peer_id_size).toHex();
         }();

         return std::make_pair(std::move(peer_info_hash),std::move(peer_id));
}

void Peer_wire_client::send_block_requests(Tcp_socket * const socket,const std::int32_t piece_idx) noexcept {
         assert(!socket->peer_choked || socket->fast_extension_enabled);
         assert(piece_idx >= 0 && piece_idx < total_piece_cnt_);
         assert(!socket->peer_bitfield.isEmpty());
         assert(socket->peer_bitfield[piece_idx]);

         const auto total_block_cnt = get_piece_info(piece_idx).total_block_cnt;
         auto & [requested_blocks,received_blocks,piece,received_block_cnt] = pieces_[piece_idx];

         if(requested_blocks.empty()){
                  requested_blocks.resize(total_block_cnt,0);
         }

         if(received_blocks.isEmpty()){
                  received_blocks.resize(total_block_cnt);
                  received_blocks.fill(false);
         }

         constexpr auto max_duplicate_requests = 1;
         constexpr auto max_request_cnt = 150;

         for(std::int32_t block_idx = 0,request_sent_cnt = 0;request_sent_cnt < max_request_cnt && block_idx < total_block_cnt;++block_idx){
                  assert(requested_blocks[block_idx] <= max_duplicate_requests);
                  assert(!requested_blocks.empty());

                  if(received_blocks[block_idx] || requested_blocks[block_idx] == max_duplicate_requests){
                           continue;
                  }

                  ++request_sent_cnt;
                  ++requested_blocks[block_idx];

                  assert(static_cast<std::int64_t>(block_idx) * max_block_size <= total_byte_cnt_);
                  socket->send_packet(craft_request_message(piece_idx,block_idx * max_block_size));

                  connect(socket,&Tcp_socket::got_choked,this,[&requested_blocks = requested_blocks,socket,block_idx]{
                           assert(socket->peer_choked);

                           if(!socket->fast_extension_enabled && !requested_blocks.empty()){
                                    assert(requested_blocks[block_idx]);
                                    --requested_blocks[block_idx];
                           }

                  },Qt::SingleShotConnection);

                  auto dec_connection = connect(socket,&Tcp_socket::disconnected,this,[&requested_blocks = requested_blocks,socket,block_idx]{

                           if(!socket->peer_choked && !requested_blocks.empty()){
                                    assert(requested_blocks[block_idx]);
                                    --requested_blocks[block_idx];
                           }
                  });

                  connect(socket,&Tcp_socket::request_rejected,this,[&requested_blocks = requested_blocks,socket,block_idx,dec_connection = std::move(dec_connection)]{
                           static_cast<void>(socket);
                           assert(socket->fast_extension_enabled);

                           if(!requested_blocks.empty()){
                                    assert(requested_blocks[block_idx]);
                                    --requested_blocks[block_idx];
                           }

                           [[maybe_unused]] const auto disconnected = disconnect(dec_connection);
                           assert(disconnected);

                  },Qt::SingleShotConnection);
         }
}

void Peer_wire_client::on_piece_request_received(Tcp_socket * const socket,const QByteArray & request) noexcept {
         const auto [piece_idx,piece_offset,byte_cnt] = extract_piece_metadata(request);

         auto send_reject_message = [socket,piece_idx = piece_idx,offset = piece_offset,byte_cnt = byte_cnt]{
                  qDebug() << "Invalid piece request" << piece_idx;

                  if(socket->fast_extension_enabled){
                           socket->send_packet(craft_reject_message(piece_idx,offset,byte_cnt));
                  }
         };

         if(piece_idx < 0 || piece_idx >= total_piece_cnt_ || piece_offset + byte_cnt > get_piece_size(piece_idx) || !bitfield_[piece_idx]){
                  return send_reject_message();
         }

         if((!socket->peer_interested || socket->am_choking) && !socket->allowed_fast_set.contains(piece_idx)){
                  return send_reject_message();
         }

         auto send_piece = [this,socket,piece_idx = piece_idx,offset = piece_offset,byte_cnt = byte_cnt](const QByteArray & piece){
                  tracker_->set_upload_byte_count(uled_byte_cnt_ += byte_cnt);
                  qDebug() << "Sent" << piece_idx;
                  socket->send_packet(craft_piece_message(piece.sliced(offset,byte_cnt),piece_idx,offset));
         };

         if(!pieces_[piece_idx].data.isEmpty()){
                  qDebug() << "Sending piece from the buffer";
                  assert(verify_piece_hash(pieces_[piece_idx].data,piece_idx));
                  return send_piece(pieces_[piece_idx].data);
         }

         socket->send_packet(keep_alive_msg.data());

         QTimer::singleShot(0,this,[this,socket = QPointer(socket),send_piece,send_reject_message,piece_idx = piece_idx]{
                  assert(bitfield_[piece_idx]);

                  if(!pieces_[piece_idx].data.isEmpty()){
                           return send_piece(pieces_[piece_idx].data);
                  }

                  if(!socket){
                           return;
                  }

                  if(auto piece = read_from_disk(piece_idx);piece && verify_piece_hash(*piece,piece_idx)){
                           send_piece(*piece);
                           pieces_[piece_idx].data = std::move(*piece);

                           QTimer::singleShot(std::chrono::seconds(30),this,[this,piece_idx]{
                                    clear_piece(piece_idx);
                           });
                  }else{
                           qDebug() << "read from the disk failed";
                           send_reject_message();
                  }
         });
}

[[nodiscard]]
std::optional<std::pair<qsizetype,qsizetype>> Peer_wire_client::get_beginning_file_info(const std::int32_t piece_idx) const noexcept {
         assert(piece_idx >= 0 && piece_idx < total_piece_cnt_);

         for(qsizetype file_handle_idx = 0,file_size_sum = 0;file_handle_idx < file_handles_.size();++file_handle_idx){
                  const auto file_size_sum_old = file_size_sum;

                  file_size_sum += get_file_size(file_handle_idx);
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

                           assert(file_offset >= 0 && file_offset < get_file_size(file_handle_idx));

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
         assert(beg_file_offset >= 0 && beg_file_offset < get_file_size(beg_file_handle_idx));

         const auto beg_file_byte_cnt = get_file_size(beg_file_handle_idx) - beg_file_offset;
         assert(beg_file_byte_cnt);

         for(std::int64_t written_byte_cnt = 0,file_handle_idx = beg_file_handle_idx;written_byte_cnt < received_piece.size();++file_handle_idx){
                  assert(file_handle_idx < file_handles_.size());

                  auto * const file_handle = file_handles_[file_handle_idx];
                  file_handle->seek(written_byte_cnt ? 0 : beg_file_offset);

                  const auto to_write_byte_cnt = std::min(received_piece.size() - written_byte_cnt,written_byte_cnt ? get_file_size(file_handle_idx) : beg_file_byte_cnt);
                  assert(to_write_byte_cnt > 0);

                  assert(file_handle->pos() + to_write_byte_cnt <= get_file_size(file_handle_idx));
                  const auto newly_written_byte_cnt = file_handle->write(received_piece.sliced(written_byte_cnt,to_write_byte_cnt));
                  assert(file_handle->pos() <= get_file_size(file_handle_idx));

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

         return resultant_piece;
}

[[nodiscard]]
bool Peer_wire_client::is_valid_reply(Tcp_socket * const socket,const QByteArray & reply,const Message_Id received_msg_id) noexcept {
         constexpr auto max_msg_id = 17;

         if(static_cast<std::int32_t>(received_msg_id) > max_msg_id){
                  qDebug() << "peer sent out of range id" << received_msg_id;
                  return false;
         }

         constexpr auto pseudo = 0;

         constexpr std::array<std::int32_t,max_msg_id + 1> expected_reply_sizes {
                  1, // choke
                  1, // unchoke
                  1, // interested
                  1, // uninterested
                  5, // have
                  pseudo,
                  13, // request
                  pseudo,
                  13, // cancel
                  pseudo,pseudo,pseudo,pseudo,
                  5, // suggest piece
                  1, // have all
                  1, // have none
                  13, // reject reply
                  5  // allowed fast
         };

         switch(received_msg_id){

                  case Message_Id::Bitfield : {
                           constexpr auto min_bitfield_msg_size = 1;
                           return reply.size() >= min_bitfield_msg_size;
                  }

                  case Message_Id::Piece : {
                           constexpr auto min_piece_msg_size = 9;
                           return reply.size() >= min_piece_msg_size;
                  }

                  default : {
                           const auto expected_size = expected_reply_sizes[static_cast<std::size_t>(received_msg_id)];
                           
                           if(expected_size == pseudo){
                                    qDebug() << "peer sent invalid message id" << received_msg_id;
                                    return false;
                           }

                           if(static_cast<std::int32_t>(received_msg_id) >= static_cast<std::int32_t>(Message_Id::Suggest_Piece) && !socket->fast_extension_enabled){
                                    qDebug() << "peer sent fast ext ids without enabling ext";
                                    return false;
                           }

                           return reply.size() == expected_size;
                  }
         }

         __builtin_unreachable();
}

void Peer_wire_client::write_settings() const noexcept {
         QSettings settings;
         settings.beginGroup("torrent_downloads");
         settings.beginGroup(QString(dl_path_).replace('/','\x20'));
         settings.setValue("bitfield",bitfield_);
         settings.setValue("uploaded_byte_count",QVariant::fromValue(uled_byte_cnt_));
}

void Peer_wire_client::read_settings() noexcept {
         QSettings settings;
         settings.beginGroup("torrent_downloads");
         settings.beginGroup(QString(dl_path_).replace('/','\x20'));

         bitfield_ = qvariant_cast<QBitArray>(settings.value("bitfield"));

         if(bitfield_.isEmpty()){
                  bitfield_.resize(total_piece_cnt_ + spare_bit_cnt_);
                  bitfield_.fill(false);
         }

         uled_byte_cnt_ = qvariant_cast<std::int64_t>(settings.value("uploaded_byte_count"));
         
         if(uled_byte_cnt_){
                  tracker_->set_upload_byte_count(uled_byte_cnt_);
         }

         assert(uled_byte_cnt_ >= 0);
}

QSet<std::int32_t> Peer_wire_client::generate_allowed_fast_set(const std::uint32_t peer_ip,const std::int32_t total_piece_cnt) noexcept {

         auto rand_bytes = [peer_ip]{
                  constexpr auto extraction_constant = 0xffffff00;
                  return util::conversion::convert_to_hex(extraction_constant & peer_ip);
         }();

         constexpr auto allowed_fast_set_size = 10;
         QSet<std::int32_t> allowed_fast_set;

         while(allowed_fast_set.size() < allowed_fast_set_size){
                  rand_bytes = QCryptographicHash::hash(rand_bytes,QCryptographicHash::Algorithm::Sha1);
                  assert(rand_bytes.size() == 20);

                  for(std::int32_t offset = 0;allowed_fast_set.size() < allowed_fast_set_size && offset < allowed_fast_set_size;offset += 4){
                           assert(offset + 4 < rand_bytes.size());
                           const auto allowed_fast_idx = util::extract_integer<std::uint32_t>(rand_bytes,offset) % static_cast<std::uint32_t>(total_piece_cnt);
                           allowed_fast_set.insert(static_cast<std::int32_t>(allowed_fast_idx));
                  }
         }

         return allowed_fast_set;
}

void Peer_wire_client::clear_piece(const std::int32_t piece_idx) noexcept {
         assert(piece_idx >= 0 && piece_idx < total_piece_cnt_);
         auto & [requested_blocks,received_blocks,piece_data,received_block_cnt] = pieces_[piece_idx];

         piece_data.clear();
         requested_blocks.clear();
         received_blocks.clear();

         piece_data.shrink_to_fit();
         requested_blocks.shrink_to_fit();
         assert(received_blocks.isEmpty());
}

void Peer_wire_client::send_pending_request(Tcp_socket * const socket) noexcept {
         assert(!socket->peer_choked);

         for(const auto pending_piece : socket->pending_pieces){

                  if(!bitfield_[pending_piece]){
                           return send_block_requests(socket,pending_piece);
                  }
         }
}

void Peer_wire_client::on_have_message_received(Tcp_socket * const socket,const std::int32_t peer_have_piece_idx) noexcept {
         assert(!socket->peer_bitfield.isEmpty());

         if(peer_have_piece_idx < 0 || peer_have_piece_idx >= total_piece_cnt_){
                  qDebug() << "peer sent invalid have index";
                  return socket->on_invalid_peer_reply();
         }

         socket->peer_bitfield[peer_have_piece_idx] = true;

         if(bitfield_[peer_have_piece_idx]){
                  return;
         }

         if(socket->fast_extension_enabled && socket->peer_allowed_fast_set.contains(peer_have_piece_idx)){
                  emit socket->fast_have_msg_received(peer_have_piece_idx);
         }else if(!socket->peer_choked){
                  send_block_requests(socket,peer_have_piece_idx);
         }else{
                  socket->pending_pieces.insert(peer_have_piece_idx);
                  
                  if(!socket->am_interested){
                           socket->am_interested = true;
                           socket->send_packet(interested_msg.data());
                  }
         }
}

void Peer_wire_client::on_bitfield_received(Tcp_socket * const socket) noexcept {
         assert(socket->peer_choked);
         assert(bitfield_.size() == socket->peer_bitfield.size());

         for(qsizetype piece_idx = 0;piece_idx < bitfield_.size();++piece_idx){

                  if(piece_idx >= total_piece_cnt_ && socket->peer_bitfield[piece_idx]){
                           qDebug() << "Spare bit was set";
                           return socket->abort();
                  }

                  if(socket->peer_bitfield[piece_idx] && !bitfield_[piece_idx]){
                           socket->am_interested = true;
                           socket->pending_pieces.insert(static_cast<std::int32_t>(piece_idx));
                  }
         }

         if(socket->am_interested){
                  socket->send_packet(interested_msg.data());
         }
}

void Peer_wire_client::on_piece_downloaded(const QPointer<Tcp_socket> socket,Piece & dled_piece,const std::int32_t dled_piece_idx) noexcept {

         if(verify_piece_hash(dled_piece.data,dled_piece_idx) && write_to_disk(dled_piece.data,dled_piece_idx)){
                  qDebug() << "piece successfully downloaded" << dled_piece_idx;

                  if(socket){
                           socket->pending_pieces.remove(dled_piece_idx);
                  }

                  emit piece_verified(dled_piece_idx);

                  constexpr std::chrono::seconds memory_release_timeout(15);

                  QTimer::singleShot(memory_release_timeout,this,[this,dled_piece_idx]{
                           clear_piece(dled_piece_idx);
                  });

         }else{
                  qDebug() << "hash verification failed";
                  clear_piece(dled_piece_idx);
         }
}

void Peer_wire_client::on_piece_received(Tcp_socket * const socket,const QByteArray & reply) noexcept {

         const auto received_piece_idx = [&reply]{
                  constexpr auto msg_begin_offset = 1;
                  return util::extract_integer<std::int32_t>(reply,msg_begin_offset);
         }();

         const auto received_offset = [&reply = reply]{
                  constexpr auto piece_begin_offset = 5;
                  return util::extract_integer<std::int32_t>(reply,piece_begin_offset);
         }();

         const auto newly_received_block = [&reply = reply]{
                  constexpr auto piece_content_offset = 9;
                  return reply.sliced(piece_content_offset);
         }();

         if(received_piece_idx >= total_piece_cnt_){
                  qDebug() << "Invalid piece idx from peer";
                  return;
         }

         const auto received_block_idx = received_offset / max_block_size;
         const auto [piece_size,block_size,total_block_cnt] = get_piece_info(received_piece_idx,received_block_idx);

         if(received_block_idx >= total_block_cnt){
                  qDebug() << "Invalid block idx from peer";
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
                  received_blocks.fill(false);
         }

         if(received_blocks[received_block_idx]){
                  qDebug() << "Aready recieved" << received_piece_idx << received_offset;
                  return;
         }

         assert(received_block_idx < total_block_cnt);
         received_blocks[received_block_idx] = true;
         ++received_block_cnt;
         
         for(qsizetype block_idx = 0;block_idx < newly_received_block.size();++block_idx){
                  assert(received_offset + block_idx < piece_data.size());
                  piece_data[received_offset + block_idx] = newly_received_block[block_idx];
         }

         if(received_block_cnt == total_block_cnt){
                  assert(received_piece_idx >= 0 && received_piece_idx < total_piece_cnt_);

                  QTimer::singleShot(0,this,[this,socket = QPointer(socket),received_piece_idx]{
                           on_piece_downloaded(socket,pieces_[received_piece_idx],received_piece_idx);
                  });
         }

         QTimer::singleShot(0,this,[this,socket = QPointer(socket),received_piece_idx]{
                  
                  if(!socket || socket->peer_choked){
                           return;
                  }

                  if(!bitfield_[received_piece_idx]){
                           send_block_requests(socket,received_piece_idx);
                  }else{
                           send_pending_request(socket);
                  }
         });
}

void Peer_wire_client::on_allowed_fast_received(Tcp_socket * const socket,const std::int32_t allowed_piece_idx) noexcept {
         assert(socket->fast_extension_enabled);
         assert(!socket->peer_bitfield.isEmpty());
         
         if(allowed_piece_idx < 0 || allowed_piece_idx >= total_piece_cnt_){
                  qDebug() << "invalid allowed fast index";
                  return socket->on_invalid_peer_reply();
         }

         if(bitfield_[allowed_piece_idx]){
                  qDebug() << "Already have the allowed fast piece";
                  return;
         }

         if(socket->peer_bitfield[allowed_piece_idx]){
                  send_block_requests(socket,allowed_piece_idx);
         }else{
                  assert(!socket->peer_allowed_fast_set.contains(allowed_piece_idx));
                  socket->peer_allowed_fast_set.insert(allowed_piece_idx);
                  
                  connect(socket,&Tcp_socket::fast_have_msg_received,this,[this,socket,allowed_piece_idx](const std::int32_t peer_have_piece_idx){
                           assert(peer_have_piece_idx >= 0 && peer_have_piece_idx < total_piece_cnt_);

                           if(!bitfield_[peer_have_piece_idx] && peer_have_piece_idx == allowed_piece_idx){
                                    qDebug() << "Sending late fast piece request" << peer_have_piece_idx;
                                    send_block_requests(socket,peer_have_piece_idx);
                           }
                  });
         }
}

void Peer_wire_client::on_suggest_piece_received(Tcp_socket * const socket,const std::int32_t suggested_piece_idx) noexcept {

         if(suggested_piece_idx < 0 || suggested_piece_idx >= total_piece_cnt_){
                  qDebug() << "peer sent invalid suggest piece index" << suggested_piece_idx;
                  return socket->on_invalid_peer_reply();
         }

         if(!bitfield_[suggested_piece_idx]){
                  send_block_requests(socket,suggested_piece_idx);
         }
}


[[nodiscard]]
std::tuple<std::int32_t,std::int32_t,std::int32_t> Peer_wire_client::extract_piece_metadata(const QByteArray & reply){
         assert(reply.size() > 12);

         const auto piece_index = [&reply = reply]{
                  constexpr auto msg_begin_offset = 1;
                  return util::extract_integer<std::int32_t>(reply,msg_begin_offset);
         }();

         const auto piece_offset = [&reply = reply]{
                  constexpr auto piece_begin_offset = 5;
                  return util::extract_integer<std::int32_t>(reply,piece_begin_offset);
         }();

         const auto piece_size = [&reply = reply]{
                  constexpr auto piece_size_offset = 9;
                  return util::extract_integer<std::int32_t>(reply,piece_size_offset);
         }();

         return std::make_tuple(piece_index,piece_offset,piece_size);
}

void Peer_wire_client::on_handshake_reply_received(Tcp_socket * socket,const QByteArray & reply){
         auto peer_info = verify_handshake_reply(socket,reply);

         if(!peer_info){
                  qDebug() << "Invalid peer handshake response";
                  return socket->abort();
         }

         auto & [peer_info_hash,peer_id] = *peer_info;

         if(info_sha1_hash_ != peer_info_hash){
                  qDebug() << "peer info hash doesn't match";
                  return socket->abort();
         }

         ++active_connection_cnt_;

         socket->peer_id = std::move(peer_id);
         socket->handshake_done = true;
         socket->peer_bitfield = QBitArray(bitfield_.size());

         if(socket->fast_extension_enabled){
                  socket->allowed_fast_set = generate_allowed_fast_set(socket->peerAddress().toIPv4Address(),total_piece_cnt_);

                  for(const auto fast_piece_idx : socket->allowed_fast_set){
                           assert(fast_piece_idx >= 0 && fast_piece_idx < total_piece_cnt_);
                           socket->send_packet(craft_allowed_fast_message(fast_piece_idx));
                  }
         }

         qDebug() << dled_piece_cnt_ << total_piece_cnt_;

         if(dled_piece_cnt_ == total_piece_cnt_ && socket->fast_extension_enabled){
                  qDebug() << "Sending have all message";
                  return socket->send_packet(have_all_msg.data());
         }

         if(!dled_piece_cnt_){

                  if(socket->fast_extension_enabled){
                           qDebug() << "Sending have none msg";
                           socket->send_packet(have_none_msg.data());
                  }

                  return;
         }

         if(constexpr auto max_have_msgs = 5;dled_piece_cnt_ <= max_have_msgs){
                  qDebug() << "Sending individual have messages";
                  
                  for(std::int32_t piece_idx = 0;piece_idx < total_piece_cnt_;++piece_idx){

                           if(bitfield_[piece_idx]){
                                    socket->send_packet(craft_have_message(piece_idx));
                           }
                  }
         }else{
                  qDebug() << "Sending bitfield";
                  socket->send_packet(craft_bitfield_message(bitfield_));
         }
}

void Peer_wire_client::communicate_with_peer(Tcp_socket * const socket){
         const auto reply_opt = socket->receive_packet();

         if(!reply_opt){
                  return;
         }

         const auto & [reply_size,reply] = *reply_opt;
         assert(reply_size && reply_size == reply.size());
         
         if(!socket->handshake_done){
                  return on_handshake_reply_received(socket,reply);
         }
 
         const auto received_msg_id = [&reply = reply]{
                  constexpr auto msg_id_offset = 0;
                  return static_cast<Message_Id>(util::extract_integer<std::int8_t>(reply,msg_id_offset));
         }();

         if(!is_valid_reply(socket,reply,received_msg_id)){
                  qDebug() << "Invalid peer reply" << received_msg_id << reply << reply.size();
                  return socket->abort();
         }

         qDebug() << received_msg_id << active_connection_cnt_;

         switch(received_msg_id){

                  case Message_Id::Choke : {
                           socket->peer_choked = true;
                           emit socket->got_choked();
                           break;
                  }

                  case Message_Id::Unchoke : {
                           socket->peer_choked = false;
                           send_pending_request(socket);
                           break;
                  }

                  case Message_Id::Interested : {
                           socket->peer_interested = true;
                           socket->am_choking = false;
                           // todo: unchoke only if the condtions are met
                           socket->send_packet(unchoke_msg.data());
                           break;
                  }

                  case Message_Id::Uninterested : {
                           socket->peer_interested = false;
                           // todo: figure
                           socket->send_packet(choke_msg.data());
                           break;
                  }

                  case Message_Id::Have : {
                           constexpr auto msg_offset = 1;
                           on_have_message_received(socket,util::extract_integer<std::int32_t>(reply,msg_offset));
                           break;
                  }

                  case Message_Id::Bitfield : {
                           constexpr auto msg_begin_offset = 1;
                           socket->peer_bitfield = util::conversion::convert_to_bits(reply.sliced(msg_begin_offset,reply_size - 1));
                           on_bitfield_received(socket);
                           break;
                  }

                  case Message_Id::Request : {
                           on_piece_request_received(socket,reply);
                           break;
                  }

                  case Message_Id::Piece : {
                           on_piece_received(socket,reply);
                           break;
                  }

                  case Message_Id::Cancel : {
                           // todo
                           // const auto [cancelled_piece_idx,cancelled_piece_offset,cancelled_piece_size] = extract_piece_metadata();
                           break;
                  }

                  case Message_Id::Have_All : {
                           assert(!socket->peer_bitfield.isEmpty());
                           socket->peer_bitfield.fill(true);
                           assert(bitfield_.size() - spare_bit_cnt_ >= 0);
                           socket->peer_bitfield.fill(false,socket->peer_bitfield.size() - spare_bit_cnt_,socket->peer_bitfield.size());
                           on_bitfield_received(socket);
                           break;
                  }

                  case Message_Id::Have_None : {
                           // no op. socket's peer bitfield is null already
                           break;
                  }

                  case Message_Id::Reject_Request : {
                           emit socket->request_rejected();
                           break;
                  }

                  case Message_Id::Allowed_Fast : {
                           constexpr auto msg_offset = 1;
                           on_allowed_fast_received(socket,util::extract_integer<std::int32_t>(reply,msg_offset));
                           break;
                  }

                  case Message_Id::Suggest_Piece : {
                           constexpr auto msg_offset = 1;
                           on_suggest_piece_received(socket,util::extract_integer<std::int32_t>(reply,msg_offset));
                           break;
                  }

                  default : {
                           __builtin_unreachable();
                  }
         }
}