#include "udp_torrent_client.hxx"
#include "peer_wire_client.hxx"
#include "download_tracker.hxx"
#include "magnet_uri_parser.hxx"

#include <QBigEndianStorageType>
#include <QNetworkDatagram>
#include <QPointer>

Udp_torrent_client::Udp_torrent_client(bencode::Metadata torrent_metadata,util::Download_resources resources,QObject * const parent)
         : QObject(parent)
         , torrent_metadata_(std::move(torrent_metadata))
         , info_sha1_hash_(calculate_info_sha1_hash(torrent_metadata_))
         , peer_client_(torrent_metadata_,{std::move(resources.dl_path),std::move(resources.file_handles),resources.tracker},id,info_sha1_hash_)
         , tracker_(resources.tracker)
{
         configure_default_connections();

         {
                  assert(!torrent_metadata_.announce_url.empty());
                  auto & tracker_urls = torrent_metadata_.announce_url_list;

                  if(std::find(tracker_urls.begin(),tracker_urls.end(),torrent_metadata_.announce_url) == tracker_urls.end()){
                           tracker_urls.insert(tracker_urls.begin(),torrent_metadata_.announce_url);
                  }
         }
}

void Udp_torrent_client::configure_default_connections() noexcept {

         connect(this,&Udp_torrent_client::announce_reply_received,[&peer_client_ = peer_client_,&event_ = event_](const Announce_reply & reply){
                  event_ = Event::Started;
                  assert(!reply.peer_urls.empty());
                  peer_client_.connect_to_peers(reply.peer_urls);
         });

         connect(&peer_client_,&Peer_wire_client::existing_pieces_verified,this,&Udp_torrent_client::send_connect_request);
         connect(tracker_,&Download_tracker::request_satisfied,this,&Udp_torrent_client::deleteLater);
}

void Udp_torrent_client::send_connect_request() noexcept {
         assert(!torrent_metadata_.announce_url_list.empty());
         assert(tracker_url_idx_ <= static_cast<qsizetype>(torrent_metadata_.announce_url_list.size()));

         if(tracker_url_idx_ == static_cast<qsizetype>(torrent_metadata_.announce_url_list.size())){
                  tracker_url_idx_ = 0;
         }

         const auto & tracker_url = torrent_metadata_.announce_url_list[static_cast<std::size_t>(tracker_url_idx_)];
         auto * const socket = new Udp_socket(QUrl(tracker_url.data()),craft_connect_request(),this);
         
         connect(socket,&Udp_socket::readyRead,this,[this,socket]{

                  auto recall_if_unread = [this,socket = QPointer(socket)]{

                           if(socket && socket->bytesAvailable()){

                                    QTimer::singleShot(0,this,[this,socket]{

                                             if(socket && socket->bytesAvailable()){
                                                      assert(socket->hasPendingDatagrams());
                                                      communicate_with_tracker(socket);
                                             }
                                    });
                           }
                  };

                  try {
                           communicate_with_tracker(socket);
                  }catch(const std::exception & exception){
                           qDebug() << exception.what();
                           return socket->abort();
                  }

                  recall_if_unread();
         });

         connect(socket,&Udp_socket::connection_timed_out,this,[this]{
                  ++tracker_url_idx_;
                  send_connect_request();
         });
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_connect_request() noexcept {
         using util::conversion::convert_to_hex;

         auto connect_request = []{
                  constexpr std::int64_t protocol_constant = 0x41727101980;
                  return convert_to_hex(protocol_constant);
         }();

         constexpr auto connect_request_size = 32;
         connect_request.reserve(connect_request_size);

         connect_request += convert_to_hex(static_cast<std::int32_t>(Action_Code::Connect));

         connect_request += []{
                  const auto txn_id = random_id_range(random_generator);
                  return convert_to_hex(txn_id);
         }();

         assert(connect_request.size() == connect_request_size);
         return connect_request;
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_announce_request(const std::int64_t tracker_connection_id) const noexcept {
         using util::conversion::convert_to_hex;
         
         auto announce_request = convert_to_hex(tracker_connection_id);
         constexpr auto announce_request_size = 196;
         announce_request.reserve(announce_request_size);

         announce_request += convert_to_hex(static_cast<std::int32_t>(Action_Code::Announce));

         announce_request += []{
                  const auto txn_id = random_id_range(random_generator);
                  return convert_to_hex(txn_id);
         }();

         announce_request += info_sha1_hash_;
         announce_request += id;

         announce_request += convert_to_hex(peer_client_.downloaded_byte_count());
         announce_request += convert_to_hex(peer_client_.remaining_byte_count());
         announce_request += convert_to_hex(peer_client_.uploaded_byte_count());

         announce_request += convert_to_hex(static_cast<std::int32_t>(event_));

         announce_request += []{
                  constexpr std::int32_t default_ip_address = 0;
                  return convert_to_hex(default_ip_address);
         }();

         announce_request += []{
                  const auto random_peer_key = random_id_range(random_generator);
                  return convert_to_hex(random_peer_key);
         }();

         announce_request += []{
                  constexpr std::int32_t default_num_want = -1;
                  return convert_to_hex(default_num_want);
         }();

         announce_request += []{
                  // todo: consider the range
                  static std::uniform_int_distribution<std::uint16_t> port_dist(10'000);
                  const auto default_port = port_dist(random_generator);
                  return convert_to_hex(default_port);
         }();

         assert(announce_request.size() == announce_request_size);
         return announce_request;
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_scrape_request(const std::int64_t tracker_connection_id) const noexcept {
         using util::conversion::convert_to_hex;
         
         auto scrape_request = convert_to_hex(tracker_connection_id);
         const auto scrape_request_size = 72;
         scrape_request.reserve(scrape_request_size);

         scrape_request += convert_to_hex(static_cast<std::int32_t>(Action_Code::Scrape));

         scrape_request += []{
                  const auto txn_id = random_id_range(random_generator);
                  return convert_to_hex(txn_id);
         }();

         scrape_request += info_sha1_hash_;

         assert(scrape_request.size() == scrape_request_size);
         return scrape_request;
}

[[nodiscard]]
std::optional<Udp_torrent_client::Announce_reply> Udp_torrent_client::extract_announce_reply(const QByteArray & reply,const std::int32_t sent_txn_id){

         if(!verify_txn_id(reply,sent_txn_id)){
                  return {};
         }

         const auto interval_time = [&reply]{
                  constexpr auto interval_offset = 8;
                  return util::extract_integer<std::int32_t>(reply,interval_offset);
         }();

         const auto leecher_cnt = [&reply]{
                  constexpr auto leechers_offset = 12;
                  return util::extract_integer<std::int32_t>(reply,leechers_offset);
         }();

         const auto seed_cnt = [&reply]{
                  constexpr auto seeders_offset = 16;
                  return util::extract_integer<std::int32_t>(reply,seeders_offset);
         }();

         auto peer_urls = [&reply]{
                  QList<QUrl> peer_urls_ret;

                  constexpr auto peers_ip_offset = 20;
                  constexpr auto peer_url_byte_cnt = 6;

                  for(qsizetype idx = peers_ip_offset;idx < reply.size();idx += peer_url_byte_cnt){
                           constexpr auto ip_byte_cnt = 4;
                           
                           const auto peer_ip = util::extract_integer<std::uint32_t>(reply,idx);
                           const auto peer_port = util::extract_integer<std::uint16_t>(reply,idx + ip_byte_cnt);

                           auto & url = peer_urls_ret.emplace_back();

                           url.setHost(QHostAddress(peer_ip).toString());
                           url.setPort(peer_port);
                           assert(url.isValid());
                  }
                  
                  return peer_urls_ret;
         }();

         return Announce_reply{std::move(peer_urls),interval_time,leecher_cnt,seed_cnt};
}

[[nodiscard]]
std::optional<Udp_torrent_client::Swarm_metadata> Udp_torrent_client::extract_scrape_reply(const QByteArray & reply,const std::int32_t sent_txn_id){
         
         if(!verify_txn_id(reply,sent_txn_id)){
                  return {};
         }

         const auto seed_cnt = [&reply]{
                  constexpr auto seed_cnt_offset = 8;
                  return util::extract_integer<std::int32_t>(reply,seed_cnt_offset);
         }();

         const auto completed_cnt = [&reply]{
                  constexpr auto dl_cnt_offset = 12;
                  return util::extract_integer<std::int32_t>(reply,dl_cnt_offset);
         }();

         const auto leecher_cnt = [&reply]{
                  constexpr auto leecher_cnt_offset = 16;
                  return util::extract_integer<std::int32_t>(reply,leecher_cnt_offset);
         }();

         return Swarm_metadata{seed_cnt,completed_cnt,leecher_cnt};
}

void Udp_torrent_client::communicate_with_tracker(Udp_socket * const socket){
         assert(socket->hasPendingDatagrams());

         // todo: validate the response size and use transactions
         const auto reply = socket->receiveDatagram().data();
         const auto tracker_action = static_cast<Action_Code>(util::extract_integer<std::int32_t>(reply,0));

         qDebug() << reply.size() << tracker_action;

         switch(tracker_action){

                  case Action_Code::Connect : {
                           const auto connection_id = extract_connect_reply(reply,socket->txn_id);

                           if(!connection_id){
                                    qDebug() << "Invalid connect reply";
                                    return socket->abort();
                           }

                           socket->announce_request = craft_announce_request(*connection_id);
                           socket->scrape_request = craft_scrape_request(*connection_id);
                           socket->send_initial_request(socket->announce_request,Udp_socket::State::Announce);

                           auto update_event_and_request = [this,socket = QPointer(socket),connection_id](const auto new_event){

                                    if(event_ != new_event){
                                             event_ = new_event;

                                             if(socket){
                                                      socket->announce_request = craft_announce_request(*connection_id);

                                                      QTimer::singleShot(0,socket,[socket]{
                                                               socket->send_initial_request(socket->announce_request,Udp_socket::State::Announce);
                                                      });
                                             }
                                    }
                           };

                           connect(tracker_,&Download_tracker::download_paused,this,[update_event_and_request]{
                                    update_event_and_request(Event::Stopped);
                           });

                           connect(tracker_,&Download_tracker::download_resumed,this,[update_event_and_request]{
                                    update_event_and_request(Event::Started);
                           });

                           connect(&peer_client_,&Peer_wire_client::download_finished,this,[update_event_and_request]{
                                    update_event_and_request(Event::Completed);
                           });

                           break;
                  }

                  case Action_Code::Announce : {

                           if(const auto announce_reply = extract_announce_reply(reply,socket->txn_id)){
                                    socket->start_interval_timer(std::chrono::seconds(announce_reply->interval_time));
                                    emit announce_reply_received(*announce_reply);
                           }else{
                                    qDebug() << "Invalid announce resposne";
                                    return socket->abort();
                           }

                           break;
                  }

                  case Action_Code::Scrape : {

                           if(const auto scrape_reply = extract_scrape_reply(reply,socket->txn_id)){
                                    qDebug() << scrape_reply->completed_cnt << scrape_reply->leecher_cnt << scrape_reply->seed_cnt;
                                    emit swarm_metadata_received(*scrape_reply);
                           }else{
                                    qDebug() << "Invalid scrape reply";
                                    return socket->abort();
                           }

                           break;
                  }

                  case Action_Code::Error : {

                           if(const auto tracker_error = extract_tracker_error(reply,socket->txn_id)){
                                    emit error_received(*tracker_error);
                           }else{
                                    qDebug() << "tracker can't be send the error reply without errors";
                                    return socket->abort();
                           }
                           
                           break;
                  }

                  default : {
                           return socket->abort();
                  }
         }
}

[[nodiscard]]
std::optional<QByteArray> Udp_torrent_client::extract_tracker_error(const QByteArray & reply,const std::int32_t sent_txn_id){

         if(!verify_txn_id(reply,sent_txn_id)){
                  return {};
         }

         constexpr auto error_offset = 8;
         return reply.sliced(error_offset);
}

[[nodiscard]]
std::optional<std::int64_t> Udp_torrent_client::extract_connect_reply(const QByteArray & reply,const std::int32_t sent_txn_id){

         if(!verify_txn_id(reply,sent_txn_id)){
                  return {};
         }

         constexpr auto connection_id_offset = 8;
         return util::extract_integer<std::int64_t>(reply,connection_id_offset);
}