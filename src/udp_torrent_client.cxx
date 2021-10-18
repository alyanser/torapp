#include "udp_torrent_client.hxx"
#include "peer_wire_client.hxx"
#include "download_tracker.hxx"

#include <QBigEndianStorageType>
#include <QNetworkDatagram>

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

         std::for_each(torrent_metadata_.announce_url_list.begin(),torrent_metadata_.announce_url_list.end(),[this](const auto & tracker_url){
                  auto * const socket = new Udp_socket(QUrl(tracker_url.data()),craft_connect_request(),this);
                  
                  connect(socket,&Udp_socket::readyRead,this,[this,socket]{

                           try {
                                    on_socket_ready_read(socket);
                           }catch(const std::exception & exception){
                                    qDebug() << exception.what();
                                    socket->abort();
                           }
                  });
         });
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_connect_request() noexcept {
         using util::conversion::convert_to_hex;

         auto connect_request = []{
                  constexpr auto protocol_constant = 0x41727101980;
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
                  constexpr auto default_ip_address = 0;
                  return convert_to_hex(default_ip_address);
         }();

         announce_request += []{
                  const auto random_peer_key = random_id_range(random_generator);
                  return convert_to_hex(random_peer_key);
         }();

         announce_request += []{
                  constexpr auto default_num_want = -1;
                  return convert_to_hex(default_num_want);
         }();

         announce_request += []{
                  constexpr std::uint16_t default_port = 6889;
                  return convert_to_hex(default_port);
         }();

         assert(announce_request.size() == announce_request_size);
         return announce_request;
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_scrape_request(const bencode::Metadata & metadata,const std::int64_t tracker_connection_id) noexcept {
         using util::conversion::convert_to_hex;
         
         auto scrape_request = convert_to_hex(tracker_connection_id);
         const auto scrape_request_size = static_cast<qsizetype>(metadata.pieces.size()) + 32;
         scrape_request.reserve(scrape_request_size);

         scrape_request += convert_to_hex(static_cast<std::int32_t>(Action_Code::Scrape));

         scrape_request += []{
                  const auto txn_id = random_id_range(random_generator);
                  return convert_to_hex(txn_id);
         }();

         scrape_request += QByteArray(metadata.pieces.data(),static_cast<qsizetype>(metadata.pieces.size()));

         assert(scrape_request.size() == scrape_request_size);
         return scrape_request;
}

void Udp_torrent_client::on_socket_ready_read(Udp_socket * const socket){

         while(socket->hasPendingDatagrams()){
                  const auto reply = socket->receiveDatagram().data();
                  const auto tracker_action = static_cast<Action_Code>(util::extract_integer<std::int32_t>(reply,0));

                  switch(tracker_action){

                           case Action_Code::Connect : {
                                    const auto connection_id = extract_connect_reply(reply,socket->txn_id);

                                    if(!connection_id){
                                             qDebug() << "Invalid connect reply";
                                             return socket->abort();
                                    }

                                    socket->announce_request = craft_announce_request(*connection_id);
                                    socket->scrape_request = craft_scrape_request(torrent_metadata_,*connection_id);
                                    socket->send_initial_request(socket->announce_request,Udp_socket::State::Announce);

                                    connect(tracker_,&Download_tracker::download_paused,this,[this,socket = QPointer(socket),connection_id]{

                                             {
                                                      const bool already_stopped = event_ == Event::Stopped;
                                                      event_ = Event::Stopped;
                                                      
                                                      if(!socket || already_stopped){
                                                               return;
                                                      }
                                             }

                                             socket->announce_request = craft_announce_request(*connection_id);
                                    });

                                    connect(tracker_,&Download_tracker::download_resumed,this,[this,socket = QPointer(socket),connection_id]{

                                             {
                                                      const bool already_running = event_ == Event::Started;
                                                      event_ = Event::Started;

                                                      if(!socket || already_running){
                                                               return;
                                                      }
                                             }

                                             socket->announce_request = craft_announce_request(*connection_id);
                                             socket->send_request(socket->announce_request);
                                    });

                                    break;
                           }

                           case Action_Code::Announce : {

                                    if(const auto announce_reply = extract_announce_reply(reply,socket->txn_id)){
                                             socket->start_interval_timer(std::chrono::seconds(announce_reply->interval_time));
                                             emit announce_reply_received(*announce_reply);
                                    }else{
                                             qInfo() << "Invalid announce resposne";
                                             return socket->abort();
                                    }

                                    break;
                           }

                           case Action_Code::Scrape : {

                                    if(const auto scrape_reply = extract_scrape_reply(reply,socket->txn_id)){
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

         assert(!socket->bytesAvailable());
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
                  QList<QUrl> ret_peer_urls;

                  constexpr auto peers_ip_offset = 20;
                  constexpr auto peer_url_byte_cnt = 6;

                  for(qsizetype idx = peers_ip_offset;idx < reply.size();idx += peer_url_byte_cnt){
                           constexpr auto ip_byte_cnt = 4;
                           
                           const auto peer_ip = util::extract_integer<std::uint32_t>(reply,idx);
                           const auto peer_port = util::extract_integer<std::uint16_t>(reply,idx + ip_byte_cnt);

                           auto & url = ret_peer_urls.emplace_back();

                           url.setHost(QHostAddress(peer_ip).toString());
                           url.setPort(peer_port);
                  }
                  
                  return ret_peer_urls;
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