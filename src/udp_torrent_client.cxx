#include "udp_torrent_client.hxx"
#include "peer_wire_client.hxx"
#include "download_tracker.hxx"

#include <QBigEndianStorageType>
#include <QNetworkDatagram>

void Udp_torrent_client::configure_default_connections() noexcept {

         connect(this,&Udp_torrent_client::announce_response_received,&peer_client_,[&peer_client_ = peer_client_](const Announce_response & response){
                  assert(!response.peer_urls.empty());
                  peer_client_.do_handshake(response.peer_urls);
         });

         connect(tracker,&Download_tracker::request_satisfied,this,&Udp_torrent_client::deleteLater);
}

void Udp_torrent_client::send_connect_request() noexcept {
         const auto & tracker_urls = metadata_.announce_url_list;

         assert(!tracker_urls.empty());

         for(const auto & tracker_url : tracker_urls){
                  auto * const socket = new Udp_socket(QUrl(tracker_url.data()),craft_connect_request(),this);
                  
                  connect(socket,&Udp_socket::readyRead,this,[this,socket]{

                           try {
                                    on_socket_ready_read(socket);
                                    
                           }catch(const std::exception & exception){
                                    qDebug() << exception.what();
                                    socket->disconnectFromHost();
                           }
                  });
         }
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_connect_request() noexcept {
         using util::conversion::convert_to_hex;

         QByteArray connect_request = []{
                  constexpr quint64_be protocol_constant(0x41727101980);
                  return convert_to_hex(protocol_constant);
         }();

         connect_request += convert_to_hex(static_cast<quint32_be>(static_cast<std::uint32_t>(Action_Code::Connect)));

         connect_request += []{
                  const quint32_be txn_id(random_id_range(random_generator));
                  return convert_to_hex(txn_id);
         }();

         return connect_request;
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_announce_request(const std::uint64_t tracker_connection_id) const noexcept {
         using util::conversion::convert_to_hex;
         
         QByteArray announce_request = convert_to_hex(static_cast<quint64_be>(tracker_connection_id));
         announce_request += convert_to_hex(static_cast<quint32_be>(static_cast<std::uint32_t>(Action_Code::Announce)));

         announce_request += []{
                  const quint32_be txn_id(random_id_range(random_generator));
                  return convert_to_hex(txn_id);
         }();

         announce_request += info_sha1_hash_;
         announce_request += id;
         announce_request += convert_to_hex(static_cast<quint64_be>(downloaded_));
         announce_request += convert_to_hex(static_cast<quint64_be>(left_));
         announce_request += convert_to_hex(static_cast<quint64_be>(uploaded_));
         announce_request += convert_to_hex(static_cast<quint32_be>(static_cast<std::uint32_t>(event_)));

         announce_request += []{
                  constexpr auto default_ip_address = 0;
                  return convert_to_hex(default_ip_address);
         }();

         announce_request += []{
                  const quint32_be random_key(random_id_range(random_generator));
                  return convert_to_hex(random_key);
         }();

         announce_request += []{
                  constexpr qint32_be default_num_want(-1);
                  return convert_to_hex(default_num_want);
         }();

         announce_request += []{
                  constexpr quint32_be default_port(6881);
                  return convert_to_hex(default_port);
         }();

         return announce_request;
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_scrape_request(const bencode::Metadata & metadata,const std::uint64_t tracker_connection_id) noexcept {
         using util::conversion::convert_to_hex;
         
         auto scrape_request = convert_to_hex(static_cast<quint64_be>(tracker_connection_id));

         scrape_request += convert_to_hex(static_cast<quint32_be>(static_cast<std::uint32_t>(Action_Code::Scrape)));

         scrape_request += []{
                  const quint32_be txn_id(random_id_range(random_generator));
                  return convert_to_hex(txn_id);
         }();

         scrape_request += QByteArray(metadata.pieces.data(),static_cast<std::ptrdiff_t>(metadata.pieces.size()));
         
         return scrape_request;
}

[[nodiscard]]
bool Udp_torrent_client::verify_txn_id(const QByteArray & response,std::uint32_t sent_txn_id){
         constexpr auto txn_id_offset = 4;
         const auto received_txn_id = util::extract_integer<std::uint32_t>(response,txn_id_offset);
         return sent_txn_id == received_txn_id;
}

void Udp_torrent_client::on_socket_ready_read(Udp_socket * const socket){

         auto on_tracker_action_connect = [this,socket](const QByteArray & tracker_response){

                  if(const auto connection_id = extract_connect_response(tracker_response,socket->txn_id())){
                           socket->set_announce_request(craft_announce_request(*connection_id));
                           socket->set_scrape_request(craft_scrape_request(metadata_,*connection_id));
                           socket->send_initial_request(socket->announce_request(),Udp_socket::State::Announce);
                  }
         };

         auto on_tracker_action_announce = [this,socket](const QByteArray & response){

                  if(const auto announce_response = extract_announce_response(response,socket->txn_id())){
                           socket->start_interval_timer(std::chrono::seconds(announce_response->interval_time));
                           emit announce_response_received(*announce_response);
                  }else{
                           socket->disconnectFromHost();
                  }
         };

         auto on_tracker_action_scrape = [this,socket](const QByteArray & response){

                  if(const auto scrape_response = extract_scrape_response(response,socket->txn_id())){
                           emit swarm_metadata_received(*scrape_response);
                  }else{
                           socket->disconnectFromHost();
                  }
         };

         auto on_tracker_action_error = [this,socket](const QByteArray & response){

                  if(const auto tracker_error = extract_tracker_error(response,socket->txn_id())){
                           emit error_received(*tracker_error);
                  }else{
                           socket->disconnectFromHost();
                  }
         };

         while(socket->hasPendingDatagrams()){
                  const auto response = socket->receiveDatagram().data();

                  const auto tracker_action = [&response]{
                           constexpr auto action_offset = 0;
                           return static_cast<Action_Code>(util::extract_integer<std::uint32_t>(response,action_offset));
                  }();

                  switch(tracker_action){
                           case Action_Code::Connect : {
                                    on_tracker_action_connect(response); 
                                    break;
                           }

                           case Action_Code::Announce : {
                                    on_tracker_action_announce(response);
                                    break;
                           }

                           case Action_Code::Scrape : {
                                    on_tracker_action_scrape(response);
                                    break;
                           }

                           case Action_Code::Error : {
                                    on_tracker_action_error(response);
                                    break;
                           }

                           default : {
                                    qInfo() << "Invalid tracker response";
                                    socket->disconnectFromHost();
                                    break;
                           }
                  }
         }

         assert(!socket->bytesAvailable());
}

[[nodiscard]]
std::optional<std::uint64_t> Udp_torrent_client::extract_connect_response(const QByteArray & response,const std::uint32_t sent_txn_id){

         if(!verify_txn_id(response,sent_txn_id)){
                  return {};
         }
         
         constexpr auto connection_id_offset = 8;
         return util::extract_integer<std::uint64_t>(response,connection_id_offset);
}

[[nodiscard]]
Udp_torrent_client::announce_optional Udp_torrent_client::extract_announce_response(const QByteArray & response,const std::uint32_t sent_txn_id){

         if(!verify_txn_id(response,sent_txn_id)){
                  return {};
         }

         const auto interval_time = [&response]{
                  constexpr auto interval_offset = 8;
                  return util::extract_integer<std::uint32_t>(response,interval_offset);
         }();

         const auto leecher_cnt = [&response]{
                  constexpr auto leechers_offset = 12;
                  return util::extract_integer<std::uint32_t>(response,leechers_offset);
         }();

         const auto seed_cnt = [&response]{
                  constexpr auto seeders_offset = 16;
                  return util::extract_integer<std::uint32_t>(response,seeders_offset);
         }();

         auto peer_urls = [&response]{
                  std::vector<QUrl> peer_urls;

                  constexpr auto peers_ip_offset = 20;
                  constexpr auto peer_url_byte_cnt = 6;

                  for(std::ptrdiff_t idx = peers_ip_offset;idx < response.size();idx += peer_url_byte_cnt){
                           constexpr auto ip_byte_cnt = 4;
                           
                           const auto peer_ip = util::extract_integer<std::uint32_t>(response,idx);
                           const auto peer_port = util::extract_integer<std::uint16_t>(response,idx + ip_byte_cnt);

                           auto & url = peer_urls.emplace_back();
                           
                           url.setHost(QHostAddress(peer_ip).toString());
                           url.setPort(peer_port);
                  }

                  return peer_urls;
         }();

         return Announce_response{std::move(peer_urls),interval_time,leecher_cnt,seed_cnt};
}

[[nodiscard]]
Udp_torrent_client::scrape_optional Udp_torrent_client::extract_scrape_response(const QByteArray & response,const std::uint32_t sent_txn_id){
         
         if(!verify_txn_id(response,sent_txn_id)){
                  return {};
         }

         const auto seed_cnt = [&response]{
                  constexpr auto seed_cnt_offset = 8;
                  return util::extract_integer<std::uint32_t>(response,seed_cnt_offset);
         }();

         const auto completed_cnt = [&response]{
                  constexpr auto download_cnt_offset = 12;
                  return util::extract_integer<std::uint32_t>(response,download_cnt_offset);
         }();

         const auto leecher_cnt = [&response]{
                  constexpr auto leecher_cnt_offset = 16;
                  return util::extract_integer<std::uint32_t>(response,leecher_cnt_offset);
         }();

         return Swarm_metadata{seed_cnt,completed_cnt,leecher_cnt};
}

[[nodiscard]]
Udp_torrent_client::error_optional Udp_torrent_client::extract_tracker_error(const QByteArray & response,const std::uint32_t sent_txn_id){

         if(!verify_txn_id(response,sent_txn_id)){
                  return {};
         }

         constexpr auto error_offset = 8;
         return response.sliced(error_offset);
}