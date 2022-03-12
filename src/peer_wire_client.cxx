#include "peer_wire_client.hxx"
#include "download_tracker.hxx"
#include "tcp_socket.hxx"
#include "magnet_url_parser.hxx"

#include <QCryptographicHash>
#include <QMessageBox>
#include <QSettings>
#include <QPointer>
#include <QFile>

Peer_wire_client::Peer_wire_client(bencode::Metadata torrent_metadata,util::Download_resources resources,QByteArray id,QByteArray info_sha1_hash)
         : properties_displayer_(torrent_metadata)
         , id_(std::move(id))
         , info_sha1_hash_(std::move(info_sha1_hash))
         , handshake_msg_(craft_handshake_message())
         , dl_path_(std::move(resources.dl_path))
         , torrent_metadata_(std::move(torrent_metadata))
         , tracker_(resources.tracker)
         , total_byte_cnt_(torrent_metadata.single_file ? torrent_metadata.single_file_size : torrent_metadata.multiple_files_size)
         , torrent_piece_size_(torrent_metadata.piece_length)
         , total_piece_cnt_(static_cast<std::int32_t>(std::ceil(static_cast<double>(total_byte_cnt_) / static_cast<double>(torrent_piece_size_))))
         , spare_piece_cnt_(total_piece_cnt_ % 8 ? 8 - total_piece_cnt_ % 8 : 0)
         , average_block_cnt_(static_cast<std::int32_t>(std::ceil(static_cast<double>(torrent_piece_size_) / max_block_size)))
         , has_metadata_(true)
         , peer_additive_bitfield_(total_piece_cnt_ + spare_piece_cnt_,0)
         , pieces_(total_piece_cnt_)
{
         assert(torrent_piece_size_ > 0);
         assert(!info_sha1_hash_.isEmpty());
         assert(!id_.isEmpty());

         file_handles_.resize(resources.file_handles.size());

         std::transform(resources.file_handles.cbegin(),resources.file_handles.cend(),file_handles_.begin(),[this](auto * const file_handle){
                  assert(file_handle->parent()); // main_window::file_allocator_
                  file_handle->setParent(this);
                  return std::make_pair(file_handle,0);
         });

         resources.file_handles.clear();
         resources.file_handles.squeeze();
         
         properties_displayer_.setup_file_info_widget(torrent_metadata_,file_handles_);
         configure_default_connections();
         read_settings();
         verify_existing_pieces();
}

Peer_wire_client::Peer_wire_client(magnet::Metadata torrent_metadata,util::Download_resources resources,QByteArray id)
         : properties_displayer_(torrent_metadata)
         , id_(std::move(id))
         , info_sha1_hash_(std::move(torrent_metadata.info_hash))
         , handshake_msg_(craft_handshake_message())
         , dl_path_(std::move(resources.dl_path))
         , tracker_(resources.tracker)
{
}

[[nodiscard]]
std::int64_t Peer_wire_client::downloaded_byte_count() const noexcept {
         return dled_byte_cnt_;
}

[[nodiscard]]
std::int64_t Peer_wire_client::uploaded_byte_count() const noexcept {
         return uled_byte_cnt_;
}

[[nodiscard]]
std::int64_t Peer_wire_client::remaining_byte_count() const noexcept {
         return total_byte_cnt_ - dled_byte_cnt_;
}

[[nodiscard]]
bool Peer_wire_client::is_valid_piece_index(const std::int32_t piece_idx) const noexcept {
         return piece_idx >= 0 && piece_idx < total_piece_cnt_;
}

[[nodiscard]]
qsizetype Peer_wire_client::file_size(const qsizetype file_idx) const noexcept {
         return static_cast<qsizetype>(torrent_metadata_.file_info[static_cast<std::size_t>(file_idx)].second);
}

void Peer_wire_client::configure_default_connections() noexcept {
         connect(this,&Peer_wire_client::piece_verified,this,&Peer_wire_client::on_piece_verified);
         connect(this,&Peer_wire_client::existing_pieces_verified,tracker_,&Download_tracker::on_verification_completed);
         connect(tracker_,&Download_tracker::properties_button_clicked,&properties_displayer_,&Torrent_properties_displayer::showMaximized);
         connect(tracker_,&Download_tracker::properties_button_clicked,&properties_displayer_,&Torrent_properties_displayer::raise);
         connect(tracker_,&Download_tracker::torrent_open_button_clicked,&properties_displayer_,&Torrent_properties_displayer::display_file_bar);
         connect(&settings_timer_,&QTimer::timeout,this,&Peer_wire_client::write_settings);

         connect(tracker_,&Download_tracker::move_files_to_trash,this,[&file_handles_ = file_handles_](){

                  std::for_each(file_handles_.cbegin(),file_handles_.cend(),[](const auto file_info){
                           file_info.first->moveToTrash();
                  });
         });

         connect(tracker_,&Download_tracker::delete_files_permanently,this,[&file_handles_ = file_handles_]{

                  std::for_each(file_handles_.cbegin(),file_handles_.cend(),[](const auto file_info){
                           file_info.first->remove();
                  });
         });

         connect(this,&Peer_wire_client::piece_verified,[&properties_displayer_ = properties_displayer_,&file_handles_ = file_handles_]{
                  
                  for(qsizetype file_idx = 0;file_idx < file_handles_.size();++file_idx){
                           const auto file_dled_byte_cnt = file_handles_[file_idx].second;
                           properties_displayer_.update_file_info(file_idx,file_dled_byte_cnt);
                  }
         });

         connect(this,&Peer_wire_client::existing_pieces_verified,[this]{
                  state_ = remaining_byte_count() ? State::Leecher : State::Seed;
                  settings_timer_.start(std::chrono::seconds(1));
         });

         request_timer_.callOnTimeout(this,[this]{
                  assert(session_uled_byte_cnt_ >= 0 && session_dled_byte_cnt_ >= 0);
                  send_requests();
         });
}

void Peer_wire_client::connect_to_peers(const QList<QUrl> & peer_urls) noexcept {
         assert(!peer_urls.isEmpty());
         qDebug() << "peers sent from the tracker" << peer_urls.size();

         std::for_each(peer_urls.cbegin(),peer_urls.cend(),[this](const auto & peer_url){

                  if(!active_peers_.contains(peer_url)){
                           qDebug() << peer_url.host() << peer_url.port() << peer_url.isValid();
                           auto * const socket = new Tcp_socket(peer_url,torrent_piece_size_,this);

                           connect(socket,&Tcp_socket::connected,this,[this,socket]{
                                    on_socket_connected(socket);
                           });
                  }
         });
}

[[nodiscard]]
std::int32_t Peer_wire_client::piece_size(const std::int32_t piece_idx) const noexcept {
         assert(is_valid_piece_index(piece_idx));
         const auto piece_size = piece_idx == total_piece_cnt_ - 1 && total_byte_cnt_ % torrent_piece_size_ ? total_byte_cnt_ % torrent_piece_size_ : torrent_piece_size_;
         assert(piece_size > 0 && piece_size <= torrent_piece_size_);
         return static_cast<std::int32_t>(piece_size);
}

void Peer_wire_client::on_piece_verified(const std::int32_t verified_piece_idx) noexcept {
         assert(is_valid_piece_index(verified_piece_idx));
         assert(!bitfield_.isEmpty());
         bitfield_[verified_piece_idx] = true;
         
         dled_byte_cnt_ += piece_size(verified_piece_idx);
         assert(dled_byte_cnt_ <= total_byte_cnt_);

         tracker_->download_progress_update(dled_byte_cnt_,total_byte_cnt_);

         if(++dled_piece_cnt_ == total_piece_cnt_){
                  assert(!remaining_byte_count());
                  state_ = State::Seed;
                  request_timer_.stop();
                  tracker_->set_error_and_finish(Download_tracker::Error::Null);
                  emit download_finished();
         }

         assert(dled_piece_cnt_ <= total_piece_cnt_);
}

[[nodiscard]]
bool Peer_wire_client::verify_piece_hash(const QByteArray & received_piece,const std::int32_t piece_idx) const noexcept {
         assert(is_valid_piece_index(piece_idx));
         constexpr auto sha1_hash_byte_cnt = 20;

         assert(torrent_metadata_.pieces.size() % sha1_hash_byte_cnt == 0);
         assert(static_cast<qsizetype>(piece_idx * sha1_hash_byte_cnt) < static_cast<qsizetype>(torrent_metadata_.pieces.size()));

         const auto beg_hash_idx = piece_idx * sha1_hash_byte_cnt;
         const QByteArray piece_hash(torrent_metadata_.pieces.substr(static_cast<std::size_t>(beg_hash_idx),sha1_hash_byte_cnt).data(),sha1_hash_byte_cnt);

         return piece_hash == QCryptographicHash::hash(received_piece,QCryptographicHash::Algorithm::Sha1);
}

[[nodiscard]]
Peer_wire_client::Piece_metadata Peer_wire_client::piece_info(const std::int32_t piece_idx,const std::int32_t offset) const noexcept {
         assert(is_valid_piece_index(piece_idx));
         assert(offset >= 0);
         assert(offset < torrent_piece_size_);

         const auto cur_piece_size = piece_size(piece_idx);
         const auto block_cnt = static_cast<std::int32_t>(std::ceil(static_cast<double>(cur_piece_size) / max_block_size));
         assert(offset / max_block_size < block_cnt);

         const auto block_size = offset / max_block_size == block_cnt - 1 && cur_piece_size % max_block_size ? cur_piece_size % max_block_size : max_block_size;

         return {cur_piece_size,block_size,block_cnt};
}

void Peer_wire_client::fill_target_piece_indexes() noexcept {
         assert(remaining_byte_count());
         assert(dled_piece_cnt_ >= 0 && dled_piece_cnt_ < total_piece_cnt_);
         assert(!peer_additive_bitfield_.isEmpty());
         assert(!bitfield_.isEmpty());
         assert(target_piece_idxes_.isEmpty());

         const auto choose_min_freq_piece = dled_piece_cnt_ > 1;

         for(constexpr auto max_target_piece_cnt = 2;target_piece_idxes_.size() < max_target_piece_cnt;){
                  std::optional<std::int32_t> target_piece_idx;

                  for(std::int32_t piece_idx = 0;piece_idx < total_piece_cnt_;++piece_idx){
                           
                           if(bitfield_[piece_idx] || target_piece_idxes_.contains(piece_idx)){
                                    continue;
                           }

                           if(!target_piece_idx){
                                    target_piece_idx = piece_idx;
                           }else{
                                    const auto target_piece_freq = peer_additive_bitfield_[*target_piece_idx];
                                    const auto piece_freq = peer_additive_bitfield_[piece_idx];

                                    if(choose_min_freq_piece ? piece_freq < target_piece_freq : piece_freq > target_piece_freq){
                                             target_piece_idx = piece_idx;
                                    }
                           }
                  }

                  if(!target_piece_idx){
                           break;
                  }

                  assert(is_valid_piece_index(*target_piece_idx));
                  assert(!bitfield_[*target_piece_idx]);

                  target_piece_idxes_.push_back(*target_piece_idx);
         }

         assert(!target_piece_idxes_.isEmpty());
         qDebug() << "Updated target piece_idxes" << target_piece_idxes_ << peer_additive_bitfield_;
}

void Peer_wire_client::verify_existing_pieces() noexcept {

         auto verify_piece = [this](auto verify_piece_callback,const std::int32_t piece_idx) -> void {
                  assert(piece_idx >= 0 && piece_idx <= total_piece_cnt_);
                  tracker_->verification_progress_update(piece_idx,total_piece_cnt_);

                  if(piece_idx < total_piece_cnt_){
                          
                           if(bitfield_[piece_idx]){  // todo: let the user decide if only torapp-downloaded pieces should be verified
                                    
                                    if(const auto piece = read_from_disk(piece_idx);piece && verify_piece_hash(*piece,piece_idx)){
                                             emit piece_verified(piece_idx);
                                    }else{
                                             qDebug() << piece_idx << "was changed on the disk";
                                             bitfield_[piece_idx] = false;
                                    }
                           }
                           
                           QTimer::singleShot(0,this,[verify_piece_callback,piece_idx]{
                                    verify_piece_callback(verify_piece_callback,piece_idx + 1);
                           });
                  }else{
                           if(remaining_byte_count()){
                                    assert(dled_piece_cnt_ >= 0 && dled_piece_cnt_ < total_piece_cnt_);
                                    tracker_->set_state(Download_tracker::State::Download);
                                    request_timer_.start(std::chrono::milliseconds(100));
                           }

                           emit existing_pieces_verified();
                  }
         };

         assert(!dled_piece_cnt_);
         assert(!dled_byte_cnt_);

         tracker_->set_state(Download_tracker::State::Verification);

         QTimer::singleShot(0,this,[verify_piece]{
                  verify_piece(verify_piece,0);
         });
}

void Peer_wire_client::on_socket_connected(Tcp_socket * const socket) noexcept {
         socket->send_packet(handshake_msg_);

         connect(tracker_,&Download_tracker::download_paused,socket,&Tcp_socket::disconnectFromHost);

         connect(socket,&Tcp_socket::readyRead,this,[this,socket]{
                  
                  if(socket->state() == Tcp_socket::SocketState::ConnectedState){
                           assert(socket->bytesAvailable());
                           on_socket_ready_read(socket);
                  }
         });

         connect(this,&Peer_wire_client::piece_verified,socket,[socket](const std::int32_t dled_piece_idx){
                  socket->send_packet(craft_have_message(dled_piece_idx));
         });

         connect(socket,&Tcp_socket::disconnected,this,[this,socket]{
                  
                  if(socket->handshake_done){
                           qDebug() << "peer disconnected after doing handshake :(" << "[ Active peers:" << active_peers_.size() << ']';
                           
                           {
                                    const auto peer_idx = active_peers_.indexOf(socket->peer_url());
                                    assert(peer_idx != -1);
                                    properties_displayer_.remove_peer(static_cast<std::int32_t>(peer_idx));
                                    active_peers_.remove(peer_idx);
                           }

                           for(qsizetype piece_idx = 0;piece_idx < socket->peer_bitfield.size();++piece_idx){
                                    peer_additive_bitfield_[piece_idx] -= socket->peer_bitfield[piece_idx];
                                    assert(peer_additive_bitfield_[piece_idx] >= 0);
                           }
                  }
         });
}

void Peer_wire_client::on_socket_ready_read(Tcp_socket * const socket) noexcept {
         
         auto is_valid_socket = [socket = QPointer(socket)]{
                  return socket && socket->state() == Tcp_socket::ConnectedState && socket->bytesAvailable();
         };

         if(!is_valid_socket()){
                  return;
         }

         try {
                  communicate_with_peer(socket);
         }catch(const std::exception & exception){
                  qDebug() << exception.what();
                  return socket->abort();
         }

         QTimer::singleShot(0,this,[this,socket,is_valid_socket]{

                  if(is_valid_socket()){
                           on_socket_ready_read(socket);
                  }
         });
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_allowed_fast_message(const std::int32_t piece_idx) noexcept {
         using util::conversion::convert_to_hex;

         const static auto allowed_fast_msg = []{
                  constexpr auto allowed_fast_packet_size = 5;
                  return convert_to_hex(allowed_fast_packet_size) + convert_to_hex(static_cast<std::int8_t>(Message_Id::Allowed_Fast));
         }();

         return allowed_fast_msg + convert_to_hex(piece_idx);
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_have_message(const std::int32_t piece_idx) noexcept {
         using util::conversion::convert_to_hex;

         const static auto have_msg = []{
                  constexpr auto have_packet_size = 5;
                  return convert_to_hex(have_packet_size) + convert_to_hex(static_cast<std::int8_t>(Message_Id::Have));
         }();

         return have_msg + convert_to_hex(piece_idx);
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_handshake_message() const noexcept {

         const static auto handshake_msg = []{
                  constexpr std::int8_t pstrlen = 19;
                  static_assert(protocol_tag.size() == pstrlen);
                  return util::conversion::convert_to_hex(pstrlen) + QByteArray(protocol_tag.data()).toHex() + reserved_bytes.data();
         }();

         assert(info_sha1_hash_.size() == 40);
         assert(id_.size() == 40);

         return handshake_msg + info_sha1_hash_ + id_;
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_piece_message(const QByteArray & piece_data,const std::int32_t piece_idx,const std::int32_t piece_offset) noexcept {
         using util::conversion::convert_to_hex;

         const static auto piece_msg = [piece_size = piece_data.size()]{
                  const auto piece_packet_size = 9 + static_cast<std::int32_t>(piece_size);
                  return convert_to_hex(piece_packet_size) + convert_to_hex(static_cast<std::int8_t>(Message_Id::Piece));
         }();

         return piece_msg + convert_to_hex(piece_idx) + convert_to_hex(piece_offset) + piece_data.toHex();
}

[[nodiscard]]
QByteArray Peer_wire_client::craft_bitfield_message(const QBitArray & bitfield) noexcept {
         using util::conversion::convert_to_hex;
         
         assert(bitfield.size() % 8 == 0);

         const static auto bitfield_msg = [&bitfield]{
                  const auto bitfield_packet_size = 1 + static_cast<std::int32_t>(bitfield.size() / 8);
                  return convert_to_hex(bitfield_packet_size) + convert_to_hex(static_cast<std::int8_t>(Message_Id::Bitfield));
         }();

         return bitfield_msg + util::conversion::convert_to_bytes(bitfield);
}

[[nodiscard]]
std::optional<std::pair<QByteArray,QByteArray>> Peer_wire_client::verify_handshake_reply(Tcp_socket * const socket,const QByteArray & reply) const noexcept {
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

                  const auto peer_protocol_tag = [&reply,protocol_label_len]{
                           constexpr auto protocol_label_offset = 1;
                           return reply.sliced(protocol_label_offset,protocol_label_len);
                  }();

                  if(peer_protocol_tag != protocol_tag.data()){
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
                  assert(peer_reserved_bits.size() == 64);

                  if(constexpr auto fast_ext_bit_idx = 61;peer_reserved_bits[fast_ext_bit_idx]){
                           socket->fast_extension_enabled = true;
                  }

                  if(constexpr auto extension_protocol_bit_idx = 43;!has_metadata_ && peer_reserved_bits[extension_protocol_bit_idx]){
                           socket->extension_protocol_enabled = true;
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
         assert(is_valid_piece_index(piece_idx));
         assert(!socket->peer_bitfield.isEmpty());
         assert(socket->peer_bitfield[piece_idx]);
         assert(!socket->peer_choked || socket->fast_extension_enabled);

         const auto total_block_cnt = piece_info(piece_idx).block_cnt;
         constexpr auto max_duplicate_requests = 2;

         auto send_block_request_impl = [this,piece_idx,total_block_cnt,socket](auto send_block_request_callback,const std::int32_t block_idx) -> void {
                  assert(block_idx >= 0 && block_idx <= total_block_cnt);

                  if(block_idx == total_block_cnt){
                           return;
                  }

                  auto & [requested_blocks,received_blocks,piece_data,received_block_cnt] = pieces_[piece_idx];

                  if(requested_blocks.empty()){
                           requested_blocks.resize(total_block_cnt,0);
                  }

                  if(received_blocks.isEmpty()){
                           received_blocks.resize(total_block_cnt);
                  }
                  
                  auto post_callback = [this,socket,send_block_request_callback,block_idx]{

                           QTimer::singleShot(0,this,[socket = QPointer(socket),send_block_request_callback,block_idx](){

                                    if(socket){
                                             send_block_request_callback(send_block_request_callback,block_idx + 1);
                                    }
                           });
                  };

                  assert(requested_blocks[block_idx] <= max_duplicate_requests);

                  if(received_blocks[block_idx] || requested_blocks[block_idx] == max_duplicate_requests){
                           return post_callback();
                  }

                  {
                           const auto piece_offset = block_idx * max_block_size;
                           assert(piece_offset <= total_byte_cnt_);
                           const auto byte_cnt = piece_info(piece_idx,piece_offset).block_size;
                           const util::Packet_metadata request_metadata{piece_idx,piece_offset,byte_cnt};

                           if(socket->is_pending_request(request_metadata) || socket->rejected_requests.contains(request_metadata) || socket->request_sent(request_metadata)){
                                    return;
                           }

                           socket->post_request(request_metadata,craft_generic_message<Message_Id::Request>(request_metadata));
                  }

                  ++requested_blocks[block_idx];

                  const auto dec_connection = connect(socket,&Tcp_socket::disconnected,this,[&requested_blocks = requested_blocks,block_idx]{

                           if(!requested_blocks.empty()){
                                    assert(requested_blocks[block_idx] > 0);
                                    --requested_blocks[block_idx];
                           }
                  });

                  connect(socket,&Tcp_socket::got_choked,this,[&requested_blocks = requested_blocks,socket,block_idx,dec_connection]{
                           assert(socket->peer_choked);

                           if(socket->state() == Tcp_socket::SocketState::ConnectedState && !socket->fast_extension_enabled && !requested_blocks.empty()){
                                    assert(requested_blocks[block_idx] > 0);
                                    --requested_blocks[block_idx];
                                    disconnect(dec_connection);
                           }

                  },Qt::SingleShotConnection);

                  auto on_request_rejected = [=,&requested_blocks = requested_blocks,decremented = false](const util::Packet_metadata rejected_request_metadata) mutable {

                           if(decremented || requested_blocks.empty() || socket->state() != Tcp_socket::SocketState::ConnectedState){
                                    return;
                           }

                           if(socket->rejected_requests.contains(rejected_request_metadata)){
                                    assert(requested_blocks[block_idx] > 0);
                                    --requested_blocks[block_idx];
                                    decremented = true;
                                    disconnect(dec_connection);
                           }
                  };

                  connect(this,&Peer_wire_client::request_rejected,socket,on_request_rejected);
                  post_callback();
         };

         QTimer::singleShot(0,this,[socket = QPointer(socket),send_block_request_impl]{

                  if(socket){
                           send_block_request_impl(send_block_request_impl,0);
                  }
         });
}

void Peer_wire_client::on_block_request_received(Tcp_socket * const socket,const QByteArray & request) noexcept {
         const auto [requested_piece_idx,requested_offset,requested_byte_cnt] = extract_packet_metadata(request);

         auto send_reject_message = [socket,piece_idx = requested_piece_idx,offset = requested_offset,byte_cnt = requested_byte_cnt]{
                  qDebug() << "Invalid piece request" << piece_idx;

                  if(socket->fast_extension_enabled){
                           socket->send_packet(craft_generic_message<Message_Id::Reject_Request>({piece_idx,offset,byte_cnt}));
                  }
         };

         if(!is_valid_piece_index(requested_piece_idx) || requested_offset + requested_byte_cnt > piece_size(requested_piece_idx) || !bitfield_[requested_piece_idx]){
                  return send_reject_message();
         }

         if((!socket->is_good_ratio() || (!socket->peer_interested || socket->am_choking)) && !socket->allowed_fast_set.contains(requested_piece_idx)){
                  return send_reject_message();
         }

         auto send_piece = [this,socket,piece_idx = requested_piece_idx,offset = requested_offset,requested_byte_cnt = requested_byte_cnt](const QByteArray & piece_to_send){
                  session_uled_byte_cnt_ += requested_byte_cnt;
                  tracker_->set_ratio(static_cast<double>(session_dled_byte_cnt_) / static_cast<double>(session_uled_byte_cnt_));
                  
                  socket->add_uploaded_bytes(requested_byte_cnt);
                  tracker_->set_upload_byte_count(uled_byte_cnt_ += requested_byte_cnt);

                  socket->send_packet(craft_piece_message(piece_to_send.sliced(offset,requested_byte_cnt),piece_idx,offset));
         };

         if(!pieces_[requested_piece_idx].data.isEmpty()){
                  qDebug() << "Sending piece" << requested_piece_idx << "from the buffer";
                  assert(verify_piece_hash(pieces_[requested_piece_idx].data,requested_piece_idx));
                  return send_piece(pieces_[requested_piece_idx].data);
         }

         socket->send_packet(keep_alive_msg.data());

         QTimer::singleShot(0,this,[this,socket = QPointer(socket),send_piece,send_reject_message,requested_piece_idx = requested_piece_idx]{
                  assert(bitfield_[requested_piece_idx]);

                  if(!socket || socket->state() != Tcp_socket::SocketState::ConnectedState){
                           return;
                  }

                  if(!pieces_[requested_piece_idx].data.isEmpty()){
                           qDebug() << "Sending piece" << requested_piece_idx << "from the buffer";
                           return send_piece(pieces_[requested_piece_idx].data);
                  }

                  if(auto piece = read_from_disk(requested_piece_idx);piece && verify_piece_hash(*piece,requested_piece_idx)){
                           pieces_[requested_piece_idx].data = std::move(*piece);
                           send_piece(pieces_[requested_piece_idx].data);

                           constexpr std::chrono::seconds piece_cleanup_timeout(15);

                           QTimer::singleShot(piece_cleanup_timeout,this,[this,requested_piece_idx]{
                                    clear_piece(requested_piece_idx);
                           });
                  }else{
                           send_reject_message();
                  }
         });
}

[[nodiscard]]
std::optional<std::pair<qsizetype,qsizetype>> Peer_wire_client::beginning_file_handle_info(const std::int32_t piece_idx) const noexcept {
         assert(is_valid_piece_index(piece_idx));

         for(qsizetype file_handle_idx = 0,file_size_sum = 0;file_handle_idx < file_handles_.size();++file_handle_idx){
                  const auto file_size_sum_old = file_size_sum;
                  file_size_sum += file_size(file_handle_idx);

                  const auto end_piece_idx = static_cast<std::int32_t>(std::ceil(static_cast<double>(file_size_sum) / static_cast<double>(torrent_piece_size_))) - 1;

                  if(end_piece_idx >= piece_idx){
                           const auto prev_offset_delta = file_size_sum_old % torrent_piece_size_ ? torrent_piece_size_ - file_size_sum_old % torrent_piece_size_ : 0;
                           assert((prev_offset_delta + file_size_sum_old) % torrent_piece_size_ == 0);

                           const auto beg_piece_idx = file_size_sum_old / torrent_piece_size_ + static_cast<bool>(prev_offset_delta);
                           assert(piece_idx >= beg_piece_idx);
                           
                           const auto prior_piece_cnt = piece_idx - beg_piece_idx;
                           assert(prior_piece_cnt >= 0);
                           const auto file_offset = prior_piece_cnt * torrent_piece_size_ + prev_offset_delta;

                           return std::make_pair(file_handle_idx,file_offset);
                  }
         }

         return {};
}

[[nodiscard]]
bool Peer_wire_client::write_to_disk(const QByteArray & received_piece,const std::int32_t received_piece_idx) noexcept {
         assert(received_piece.size() == piece_size(received_piece_idx));
         assert(verify_piece_hash(received_piece,received_piece_idx));

         const auto file_handle_info = beginning_file_handle_info(received_piece_idx);

         if(!file_handle_info){
                  return false;
         }
         
         const auto [beg_file_handle_idx,beg_file_offset] = *file_handle_info;
         assert(beg_file_handle_idx < file_handles_.size());

         const auto beg_file_byte_cnt = file_size(beg_file_handle_idx) - beg_file_offset;
         assert(beg_file_byte_cnt);

         for(std::int64_t written_byte_cnt = 0,file_handle_idx = beg_file_handle_idx;written_byte_cnt < received_piece.size();++file_handle_idx){
                  assert(file_handle_idx < file_handles_.size());

                  auto & [file_handle,file_dled_byte_cnt] = file_handles_[file_handle_idx];
                  file_handle->seek(written_byte_cnt ? 0 : beg_file_offset);

                  const auto to_write_byte_cnt = std::min(received_piece.size() - written_byte_cnt,written_byte_cnt ? file_size(file_handle_idx) : beg_file_byte_cnt);

                  assert(to_write_byte_cnt > 0);
                  assert(file_handle->pos() + to_write_byte_cnt <= file_size(file_handle_idx));

                  const auto newly_written_byte_cnt = file_handle->write(received_piece.sliced(written_byte_cnt,to_write_byte_cnt));
                  file_dled_byte_cnt += newly_written_byte_cnt;
                  assert(file_handle->pos() <= file_size(file_handle_idx));

                  if(newly_written_byte_cnt != to_write_byte_cnt){
                           qDebug() << "Could not write to file";
                           return false;
                  }

                  written_byte_cnt += newly_written_byte_cnt;
                  assert(written_byte_cnt <= received_piece.size());
                  file_handle->flush();
         }

         assert(read_from_disk(received_piece_idx));
         assert(received_piece == read_from_disk(received_piece_idx));
         return true;
}

[[nodiscard]]
std::optional<QByteArray> Peer_wire_client::read_from_disk(const std::int32_t requested_piece_idx) noexcept {
         assert(is_valid_piece_index(requested_piece_idx));

         const auto beg_file_handle_info = beginning_file_handle_info(requested_piece_idx);

         if(!beg_file_handle_info){
                  return {};
         }

         const auto requested_piece_size = piece_size(requested_piece_idx);

         QByteArray resultant_piece;
         resultant_piece.reserve(requested_piece_size);

         const auto [beg_file_handle_idx,beg_file_offset] = *beg_file_handle_info;
         assert(beg_file_handle_idx < file_handles_.size());
         
         const auto beg_file_byte_cnt = file_size(beg_file_handle_idx) - beg_file_offset;
         assert(beg_file_byte_cnt > 0);

         for(qsizetype file_handle_idx = beg_file_handle_idx;resultant_piece.size() < requested_piece_size;++file_handle_idx){

                  if(file_handle_idx == file_handles_.size()){
                           return {};
                  }

                  auto & [file_handle,file_dled_byte_cnt] = file_handles_[file_handle_idx];
                  file_handle->seek(resultant_piece.size() ? 0 : beg_file_offset);

                  const auto to_read_byte_cnt = std::min(resultant_piece.size() ? file_size(file_handle_idx) : beg_file_byte_cnt,requested_piece_size - resultant_piece.size());
                  assert(to_read_byte_cnt > 0);

                  const auto newly_read_bytes = file_handle->read(to_read_byte_cnt);


                  if(newly_read_bytes.size() != to_read_byte_cnt){
                           return {};
                  }

                  if(state_ == State::Verification){
                           file_dled_byte_cnt += newly_read_bytes.size();
                  }
                  
                  resultant_piece += newly_read_bytes;
         }

         assert(resultant_piece.size() == requested_piece_size);
         return resultant_piece;
}

[[nodiscard]]
bool Peer_wire_client::is_valid_reply(Tcp_socket * const socket,const QByteArray & reply,const Message_Id received_msg_id) noexcept {
         constexpr auto max_msg_id = 20;

         if(static_cast<std::int32_t>(received_msg_id) > max_msg_id){
                  qDebug() << "peer sent out of range id" << received_msg_id;
                  return false;
         }

         constexpr auto pseudo = 0;

         constexpr static std::array<std::int32_t,max_msg_id + 1> expected_reply_sizes {
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

                  case Message_Id::Extended_Protocol : {
                           constexpr auto min_extended_msg_size = 1;
                           return reply.size() >= min_extended_msg_size;
                  }

                  default : {
                           const auto expected_size = expected_reply_sizes[static_cast<std::size_t>(received_msg_id)];
                           
                           if(expected_size == pseudo){
                                    qDebug() << "peer sent invalid message id" << received_msg_id;
                                    return false;
                           }

                           if(static_cast<std::int32_t>(received_msg_id) >= static_cast<std::int32_t>(Message_Id::Suggest_Piece) && !socket->fast_extension_enabled){
                                    qDebug() << "peer sent fast extension ids without enabling the extension first";
                                    return false;
                           }

                           return reply.size() == expected_size;
                  }
         }
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
                  bitfield_.resize(total_piece_cnt_ + spare_piece_cnt_);
         }

         uled_byte_cnt_ = qvariant_cast<std::int64_t>(settings.value("uploaded_byte_count"));
         
         if(uled_byte_cnt_){
                  tracker_->set_upload_byte_count(uled_byte_cnt_);
         }

         assert(uled_byte_cnt_ >= 0);
}

[[nodiscard]]
QSet<std::int32_t> Peer_wire_client::generate_allowed_fast_set(const std::uint32_t peer_ip,const std::int32_t total_piece_cnt) noexcept {

         auto rand_bytes = [peer_ip]{
                  constexpr auto protocol_mask = 0xffffff00;
                  return util::conversion::convert_to_hex(peer_ip & protocol_mask);
         }();

         constexpr auto allowed_fast_set_size = 10;
         QSet<std::int32_t> allowed_fast_set;

         while(allowed_fast_set.size() < allowed_fast_set_size){
                  rand_bytes = QCryptographicHash::hash(rand_bytes,QCryptographicHash::Algorithm::Sha1);
                  assert(rand_bytes.size() == 20);

                  for(std::int32_t offset = 0;allowed_fast_set.size() < allowed_fast_set_size && offset < allowed_fast_set_size;offset += 4){
                           assert(offset + 4 < rand_bytes.size());
                           assert(total_piece_cnt);
                           const auto allowed_fast_idx = util::extract_integer<std::uint32_t>(rand_bytes,offset) % static_cast<std::uint32_t>(total_piece_cnt);
                           allowed_fast_set.insert(static_cast<std::int32_t>(allowed_fast_idx));
                  }
         }

         assert(allowed_fast_set.size() == allowed_fast_set_size);
         return allowed_fast_set;
}

void Peer_wire_client::clear_piece(const std::int32_t piece_idx) noexcept {
         assert(is_valid_piece_index(piece_idx));
         auto & [requested_blocks,received_blocks,piece_data,received_block_cnt] = pieces_[piece_idx];

         piece_data.clear();
         piece_data.squeeze();

         requested_blocks.clear();
         requested_blocks.squeeze();

         received_blocks.clear();

         received_block_cnt = 0;
}

void Peer_wire_client::on_have_message_received(Tcp_socket * const socket,const std::int32_t peer_have_piece_idx) noexcept {
         assert(!socket->peer_bitfield.isEmpty());

         if(!is_valid_piece_index(peer_have_piece_idx)){
                  qDebug() << "peer sent invalid have index";
                  return socket->on_peer_fault();
         }

         if(!socket->peer_bitfield[peer_have_piece_idx]){
                  socket->peer_bitfield[peer_have_piece_idx] = true;
                  ++peer_additive_bitfield_[peer_have_piece_idx];
         }else{
                  qDebug() << "Peer sent duplicate 'have' msg";
                  // ? consider as error?
         }
}

void Peer_wire_client::on_bitfield_received(Tcp_socket * const socket) noexcept {

         for(qsizetype piece_idx = 0;piece_idx < bitfield_.size();++piece_idx){
                  
                  if(piece_idx >= total_piece_cnt_ && socket->peer_bitfield[piece_idx]){
                           qDebug() << "Spare bit was set";
                           return socket->abort();
                  }

                  if(!socket->am_interested && socket->peer_bitfield[piece_idx] && !bitfield_[piece_idx]){
                           socket->am_interested = true;
                           socket->send_packet(interested_msg.data());
                  }

                  peer_additive_bitfield_[piece_idx] += socket->peer_bitfield[piece_idx];
         }
}

void Peer_wire_client::on_piece_downloaded(Piece & dled_piece,const std::int32_t dled_piece_idx) noexcept {

         if(verify_piece_hash(dled_piece.data,dled_piece_idx) && write_to_disk(dled_piece.data,dled_piece_idx)){
                  qDebug() << "piece successfully downloaded" << dled_piece_idx;

                  emit piece_verified(dled_piece_idx);

                  session_dled_byte_cnt_ += piece_size(dled_piece_idx);
                  tracker_->set_ratio(session_uled_byte_cnt_ ? static_cast<double>(session_dled_byte_cnt_) / static_cast<double>(session_uled_byte_cnt_) : 0);

                  if(const auto remove_idx = target_piece_idxes_.indexOf(dled_piece_idx);remove_idx != -1){
                           target_piece_idxes_.removeAt(remove_idx);

                           if(target_piece_idxes_.isEmpty() && remaining_byte_count()){
                                    fill_target_piece_indexes();
                           }
                  }

                  QTimer::singleShot(std::chrono::seconds(5),this,[this,dled_piece_idx]{
                           clear_piece(dled_piece_idx);
                  });
         }else{
                  qDebug() << "downloaded piece hash verification failed";
                  clear_piece(dled_piece_idx);
         }
}

void Peer_wire_client::on_block_received(Tcp_socket * const socket,const QByteArray & reply) noexcept {

         const auto received_piece_idx = [&reply]{
                  constexpr auto msg_begin_offset = 1;
                  return util::extract_integer<std::int32_t>(reply,msg_begin_offset);
         }();

         const auto received_piece_offset = [&reply = reply]{
                  constexpr auto piece_begin_offset = 5;
                  return util::extract_integer<std::int32_t>(reply,piece_begin_offset);
         }();

         const auto received_block = [&reply = reply]{
                  constexpr auto piece_content_offset = 9;
                  return reply.sliced(piece_content_offset);
         }();

         if(received_piece_idx < 0 || received_piece_idx >= total_piece_cnt_){
                  qDebug() << "Invalid piece idx from peer";
                  return socket->on_peer_fault();
         }

         const auto received_block_idx = received_piece_offset / max_block_size;
         const auto [piece_size,block_size,total_block_cnt] = piece_info(received_piece_idx,received_block_idx);


         if(received_block_idx >= total_block_cnt){
                  qDebug() << "Invalid block idx from peer";
                  return socket->on_peer_fault();
         }

         const util::Packet_metadata received_packet_metadata{received_piece_idx,received_piece_offset,static_cast<std::int32_t>(received_block.size())};

         if(!socket->request_sent(received_packet_metadata)){
                  qDebug() << "Peer sent a block without being requested";
                  return socket->abort();
         }

         socket->add_downloaded_bytes(received_block.size());

         assert(received_piece_idx < pieces_.size());
         auto & [requested_blocks,received_blocks,piece_data,received_block_cnt] = pieces_[received_piece_idx];

         if(piece_data.isEmpty()){
                  piece_data.resize(static_cast<qsizetype>(piece_size));
         }

         if(received_blocks.isEmpty()){
                  received_blocks.resize(total_block_cnt);
         }

         if(received_blocks[received_block_idx]){
                  qDebug() << "already recieved" << received_piece_idx << received_piece_offset << '[' << piece_size << "bytes wasted ]";
                  return;
         }

         assert(received_block_idx >= 0 && received_block_idx < total_block_cnt);
         
         received_blocks[received_block_idx] = true;

         assert(received_piece_offset + received_block.size() <= piece_data.size());
         std::move(received_block.begin(),received_block.end(),piece_data.begin() + received_piece_offset);

         if(++received_block_cnt == total_block_cnt){

                  QTimer::singleShot(0,this,[this,received_piece_idx]{
                           on_piece_downloaded(pieces_[received_piece_idx],received_piece_idx);
                  });
         }

         assert(received_block_cnt <= total_block_cnt);
         emit valid_block_received(received_packet_metadata);
}

void Peer_wire_client::on_allowed_fast_received(Tcp_socket * const socket,const std::int32_t allowed_piece_idx) noexcept {
         assert(socket->fast_extension_enabled);
         assert(!socket->peer_bitfield.isEmpty());
         
         if(allowed_piece_idx < 0 || allowed_piece_idx >= total_piece_cnt_){
                  qDebug() << "invalid allowed fast index";
                  return socket->on_peer_fault();
         }

         if(bitfield_[allowed_piece_idx]){
                  qDebug() << "already have the allowed fast piece";
                  return;
         }

         socket->peer_allowed_fast_set.insert(allowed_piece_idx);

         if(socket->peer_bitfield[allowed_piece_idx]){
                  send_block_requests(socket,allowed_piece_idx);
         }
}

void Peer_wire_client::on_suggest_piece_received(Tcp_socket * const socket,const std::int32_t suggested_piece_idx) noexcept {
         assert(socket->fast_extension_enabled);

         if(!is_valid_piece_index(suggested_piece_idx) || !socket->peer_bitfield[suggested_piece_idx]){
                  qDebug() << "peer sent invalid suggest msg" << suggested_piece_idx;
                  return socket->on_peer_fault();
         }
         
         if(!bitfield_[suggested_piece_idx]){
                  send_block_requests(socket,suggested_piece_idx);
         }
}

[[nodiscard]]
util::Packet_metadata Peer_wire_client::extract_packet_metadata(const QByteArray & reply){
         assert(reply.size() > 12);

         constexpr auto piece_idx_offset = 1;
         constexpr auto piece_begin_offset = piece_idx_offset + sizeof(std::int32_t);
         constexpr auto byte_cnt_offset = piece_begin_offset + sizeof(std::int32_t);

         const auto piece_idx = util::extract_integer<std::int32_t>(reply,piece_idx_offset);
         const auto piece_offset =  util::extract_integer<std::int32_t>(reply,piece_begin_offset);
         const auto byte_cnt = util::extract_integer<std::int32_t>(reply,byte_cnt_offset);

         return {piece_idx,piece_offset,byte_cnt};
}

void Peer_wire_client::on_handshake_reply_received(Tcp_socket * const socket,const QByteArray & reply){
         assert(socket->state() == Tcp_socket::ConnectedState);
         auto peer_info = verify_handshake_reply(socket,reply);

         if(!peer_info){
                  qDebug() << "Invalid peer handshake response";
                  return socket->abort();
         }

         auto & [peer_info_hash,peer_id] = *peer_info;

         if(info_sha1_hash_ != peer_info_hash){
                  qDebug() << "peer info hash doesn't match" << info_sha1_hash_ << peer_info_hash;
                  return socket->abort();
         }

         socket->peer_id = std::move(peer_id);
         socket->handshake_done = true;

         active_peers_.emplace_back(socket->peer_url());
         properties_displayer_.add_peer(socket);

         if(!has_metadata_){
                  return;
         }

         connect(this,&Peer_wire_client::valid_block_received,socket,[socket](const util::Packet_metadata request_metadata){
                  socket->remove_request({request_metadata.piece_idx,request_metadata.piece_offset,request_metadata.byte_cnt});
         });

         connect(this,&Peer_wire_client::send_requests,socket,[this,socket]{

                  if(socket->state() != Tcp_socket::SocketState::ConnectedState || socket->peer_bitfield.isEmpty() || target_piece_idxes_.isEmpty()){
                           return;
                  }

                  assert(!bitfield_.isEmpty());
                  assert(socket->peer_bitfield.size() == bitfield_.size());

                  if(!remaining_byte_count()){
                           assert(dled_byte_cnt_ == total_byte_cnt_);
                           assert(target_piece_idxes_.isEmpty());
                           return request_timer_.stop();
                  }

                  const auto target_piece_idx = [&target_piece_idxes_ = target_piece_idxes_]{
                           static std::mt19937 random_generator(std::random_device{}());
                           std::uniform_int_distribution<qsizetype> idx_range(0,target_piece_idxes_.size() - 1);
                           return target_piece_idxes_[idx_range(random_generator)];
                  }();

                  assert(!bitfield_[target_piece_idx]);

                  if(!socket->peer_bitfield[target_piece_idx]){
                           return;
                  }

                  if(!socket->peer_choked || socket->peer_allowed_fast_set.contains(target_piece_idx)){
                           send_block_requests(socket,target_piece_idx);
                  }else if(!socket->am_interested){
                           socket->am_interested = true;
                           socket->send_packet(interested_msg.data());
                  }
         });

         if(socket->fast_extension_enabled){

                  QTimer::singleShot(0,this,[socket = QPointer(socket),total_piece_cnt_ = total_piece_cnt_]{

                           if(!socket){
                                    return;
                           }

                           socket->allowed_fast_set = generate_allowed_fast_set(socket->peerAddress().toIPv4Address(),total_piece_cnt_);

                           std::for_each(socket->allowed_fast_set.cbegin(),socket->allowed_fast_set.cend(),[socket](const auto fast_piece_idx){
                                    socket->send_packet(craft_allowed_fast_message(fast_piece_idx));
                           });
                  });
         }

         if(!dled_piece_cnt_){

                  if(socket->fast_extension_enabled){
                           socket->send_packet(have_none_msg.data());
                  }

                  return;
         }

         if(dled_piece_cnt_ == total_piece_cnt_ && socket->fast_extension_enabled){
                  assert(!remaining_byte_count());
                  return socket->send_packet(have_all_msg.data());
         }

         if(constexpr auto max_have_msgs = 10;dled_piece_cnt_ <= max_have_msgs){

                  for(std::int32_t piece_idx = 0;piece_idx < total_piece_cnt_;++piece_idx){

                           if(bitfield_[piece_idx]){
                                    socket->send_packet(craft_have_message(piece_idx));
                           }
                  }
         }else{
                  socket->send_packet(craft_bitfield_message(bitfield_));
         }
}

template<Peer_wire_client::Message_Id msg_id>
[[nodiscard]]
QByteArray Peer_wire_client::craft_generic_message(const util::Packet_metadata packet_metadata) noexcept {
         static_assert(msg_id == Message_Id::Reject_Request || msg_id == Message_Id::Request || msg_id == Message_Id::Cancel,
                  "Only valid for Message_Id::[Reject_Request,Cancel,Request]");

         using util::conversion::convert_to_hex;

         const static auto msg = []{
                  constexpr auto packet_size = 13;
                  return convert_to_hex(packet_size) + convert_to_hex(static_cast<std::int8_t>(msg_id));
         }();

         return msg + convert_to_hex(packet_metadata.piece_idx) + convert_to_hex(packet_metadata.piece_offset) + convert_to_hex(packet_metadata.byte_cnt);
}

void Peer_wire_client::communicate_with_peer(Tcp_socket * const socket){
         assert(socket->bytesAvailable());
         const auto reply = socket->receive_packet();

         if(!reply){
                  return;
         }

         if(!socket->handshake_done){
                  return on_handshake_reply_received(socket,*reply);
         }

         const auto received_msg_id = [&reply = reply]{
                  constexpr auto msg_id_offset = 0;
                  return static_cast<Message_Id>(util::extract_integer<std::int8_t>(*reply,msg_id_offset));
         }();

         if(!is_valid_reply(socket,*reply,received_msg_id)){
                  qDebug() << "Invalid peer reply" << received_msg_id << *reply << reply->size();
                  return socket->abort();
         }

         qDebug() << received_msg_id << "[ Active peers:" << active_peers_.size() << ']';
         constexpr auto msg_begin_offset = 1;

         {
                  if(socket->extension_protocol_enabled && received_msg_id == Message_Id::Extended_Protocol){
                           on_extension_message_received(socket,reply->sliced(1) /* skip standard bit. '20' for extension protocol */);
                  }
         }

         if(!has_metadata_){
                  return;
         }
         
         switch(received_msg_id){

                  case Message_Id::Choke : {

                           if(!socket->peer_choked){
                                    socket->peer_choked = true;
                                    emit socket->got_choked();
                           }else{
                                    socket->on_peer_fault();
                           }

                           break;
                  }

                  case Message_Id::Unchoke : {

                           if(socket->peer_choked){
                                    socket->peer_choked = false;
                           }else{
                                    socket->on_peer_fault();
                           }

                           break;
                  }

                  case Message_Id::Interested : {

                           if(socket->peer_interested){
                                    socket->on_peer_fault();
                                    break;
                           }
                           
                           socket->peer_interested = true;

                           if(socket->am_choking && socket->is_good_ratio()){
                                    socket->am_choking = false;
                                    socket->send_packet(unchoke_msg.data());
                           }

                           break;
                  }

                  case Message_Id::Uninterested : {

                           if(socket->peer_interested){
                                    socket->peer_interested = false;
                           }else{
                                    socket->on_peer_fault();
                           }

                           break;
                  }

                  case Message_Id::Have : {
                           constexpr auto msg_offset = 1;
                           on_have_message_received(socket,util::extract_integer<std::int32_t>(*reply,msg_offset));
                           break;
                  }

                  case Message_Id::Bitfield : {
                           
                           if(!socket->peer_bitfield.isEmpty()){
                                    socket->on_peer_fault();
                                    break;
                           }

                           socket->peer_bitfield = util::conversion::convert_to_bits(reply->sliced(msg_begin_offset,reply->size() - 1));
                           assert(socket->peer_bitfield.size() == bitfield_.size());
                           on_bitfield_received(socket);
                           break;
                  }

                  case Message_Id::Request : {
                           on_block_request_received(socket,*reply);
                           break;
                  }

                  case Message_Id::Piece : {
                           on_block_received(socket,*reply);
                           break;
                  }

                  case Message_Id::Cancel : {
                           // todo
                           // const auto [cancelled_piece_idx,cancelled_piece_offset,canceled_byte_cnt] = extract_piece_metadata(reply);
                           break;
                  }

                  case Message_Id::Have_All : {

                           if(!socket->peer_bitfield.isEmpty()){
                                    socket->on_peer_fault();
                                    break;
                           }

                           assert(total_piece_cnt_ + spare_piece_cnt_ == bitfield_.size());
                           socket->peer_bitfield = QBitArray(bitfield_.size(),true);

                           if(spare_piece_cnt_){
                                    assert(total_piece_cnt_ < socket->peer_bitfield.size());
                                    socket->peer_bitfield.fill(false,total_piece_cnt_,socket->peer_bitfield.size());
                           }

                           on_bitfield_received(socket);
                           break;
                  }

                  case Message_Id::Have_None : {

                           if(!socket->peer_bitfield.isEmpty()){
                                    socket->on_peer_fault();
                           }else{
                                    socket->peer_bitfield = QBitArray(bitfield_.size(),false);
                           }

                           break;
                  }

                  case Message_Id::Reject_Request : {
                           const auto rejected_request_metadata = extract_packet_metadata(*reply);

                           if(socket->request_sent(rejected_request_metadata)){
                                    socket->rejected_requests.insert(rejected_request_metadata);
                                    emit request_rejected(rejected_request_metadata);
                           }else{
                                    socket->abort();
                           }

                           break;
                  }

                  case Message_Id::Allowed_Fast : {
                           on_allowed_fast_received(socket,util::extract_integer<std::int32_t>(*reply,msg_begin_offset));
                           break;
                  }

                  case Message_Id::Suggest_Piece : {
                           on_suggest_piece_received(socket,util::extract_integer<std::int32_t>(*reply,msg_begin_offset));
                           break;
                  }
         }
}

void Peer_wire_client::on_extension_message_received(Tcp_socket * const socket,QByteArray message){
         assert(socket);
         assert(socket->extension_protocol_enabled);
         assert(!message.isEmpty());

         if(const auto msg_type = util::extract_integer<std::int8_t>(message);!msg_type){ // extension handshake message

                  {        //! hacky message fix. improve later

                           message.removeIf([](const char c){
                                    return c == '"';
                           });

                           message.replace("\\x","%");
                           message = QByteArray::fromPercentEncoding(message.sliced(1) /* skip the msg id */);
                  }

                  try {
                           for(const auto & [key,value] : bencode::parse_content(message,"")){
                                    constexpr std::string_view standard_dict_name("m");

                                    if(key != standard_dict_name){
                                             continue;
                                    }

                                    for(const auto & [peer_label_heading,peer_label_idx] : std::any_cast<bencode::dictionary>(value)){

                                             if(constexpr std::string_view metadata_key("ut_metadata");peer_label_heading == metadata_key){
                                                      socket->peer_ut_metadata_id = std::any_cast<std::int64_t>(peer_label_idx);
                                             }
                                    }
                           }

                  }catch(const std::exception & exception){
                           qDebug() << "error in the extension dictionary. aborting connection" << exception.what();
                           return socket->abort();
                  }

         }else if(socket->peer_ut_metadata_id != -1){ // peer supports sharing torrent metadata
                  // todo : craft request
         }
}

template QByteArray Peer_wire_client::craft_generic_message<Peer_wire_client::Message_Id::Reject_Request>(util::Packet_metadata) noexcept;
template QByteArray Peer_wire_client::craft_generic_message<Peer_wire_client::Message_Id::Request>(util::Packet_metadata) noexcept;
template QByteArray Peer_wire_client::craft_generic_message<Peer_wire_client::Message_Id::Cancel>(util::Packet_metadata) noexcept;