#include "udp_torrent_client.hxx"

#include "peer_wire_client.hxx"

#include <QBigEndianStorageType>
#include <QNetworkDatagram>

void Udp_torrent_client::configure_default_connections() const noexcept {

	connect(this,&Udp_torrent_client::announce_response_received,[this](const Announce_response & announce_response){
		auto peer_client = std::make_shared<Peer_wire_client>(torrent_metadata_,id,info_sha1_hash_)->bind_lifetime();
		assert(!announce_response.peer_urls.empty());
		peer_client->do_handshake(announce_response.peer_urls);
	});
}

void Udp_torrent_client::send_connect_requests() noexcept {

	if(torrent_metadata_.announce_url_list.empty()){
		torrent_metadata_.announce_url_list.emplace_back(torrent_metadata_.announce_url);
	}

	for(const auto & announce_url : torrent_metadata_.announce_url_list){
		auto socket = std::make_shared<Udp_socket>(QUrl(announce_url.data()),craft_connect_request())->bind_lifetime();
		
		connect(socket.get(),&Udp_socket::readyRead,this,[this,socket = socket.get()]{

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

	connect_request += []{
		constexpr quint32_be connect_action(static_cast<std::uint32_t>(Action_Code::Connect));
		return convert_to_hex(connect_action);
	}();

	connect_request += []{
		const quint32_be txn_id(random_id_range(random_generator));
		return convert_to_hex(txn_id);
	}();

	return connect_request;
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_announce_request(const quint64_be tracker_connection_id) const noexcept {
	using util::conversion::convert_to_hex;

	QByteArray announce_request = convert_to_hex(tracker_connection_id);

	announce_request += []{
		constexpr quint32_be connect_action(static_cast<std::uint32_t>(Action_Code::Announce));
		return convert_to_hex(connect_action);
	}();

	announce_request += []{
		const quint32_be txn_id(random_id_range(random_generator));
		return convert_to_hex(txn_id);
	}();

	announce_request += info_sha1_hash_;
	announce_request += id;
	announce_request += convert_to_hex(downloaded_);
	announce_request += convert_to_hex(left_);
	announce_request += convert_to_hex(uploaded_);
	announce_request += convert_to_hex(static_cast<std::uint32_t>(event_));

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
		constexpr quint32_be default_port(6889);
		return convert_to_hex(default_port);
	}();

	return announce_request;
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_scrape_request(const bencode::Metadata & metadata,const quint64_be tracker_connection_id) noexcept {
	using util::conversion::convert_to_hex;
	
	auto scrape_request = convert_to_hex(tracker_connection_id);

	scrape_request += []{
		constexpr quint32_be scrape_action(static_cast<std::uint32_t>(Action_Code::Scrape));
		return convert_to_hex(scrape_action);
	}();

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
			socket->set_scrape_request(craft_scrape_request(torrent_metadata_,*connection_id));
			socket->send_initial_request(socket->announce_request(),Udp_socket::State::Announce);
		}
	};

	auto on_tracker_action_announce = [this,socket](const QByteArray & response){

		if(const auto announce_response = extract_announce_response(response,socket->txn_id())){
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
				socket->disconnectFromHost();
			}
		}
	}
}

[[nodiscard]]
std::optional<quint64_be> Udp_torrent_client::extract_connect_response(const QByteArray & response,const std::uint32_t sent_txn_id){

	if(!verify_txn_id(response,sent_txn_id)){
		return {};
	}
	
	constexpr auto connection_id_offset = 8;
	return quint64_be(util::extract_integer<std::uint64_t>(response,connection_id_offset));
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

	const auto leechers_count = [&response]{
		constexpr auto leechers_offset = 12;
		return util::extract_integer<std::uint32_t>(response,leechers_offset);
	}();

	const auto seeds_count = [&response]{
		constexpr auto seeders_offset = 16;
		return util::extract_integer<std::uint32_t>(response,seeders_offset);
	}();

	auto peer_urls = [&response]{
		std::vector<QUrl> peer_urls;

		constexpr auto peers_ip_offset = 20;
		constexpr auto peer_url_bytes = 6;

		for(std::ptrdiff_t idx = peers_ip_offset;idx < response.size();idx += peer_url_bytes){
			constexpr auto ip_bytes = 4;
			
			const auto peer_ip = util::extract_integer<std::uint32_t>(response,idx);
			const auto peer_port = util::extract_integer<std::uint16_t>(response,idx + ip_bytes);

			auto & url = peer_urls.emplace_back();
			
			url.setHost(QHostAddress(peer_ip).toString());
			url.setPort(peer_port);
		}

		return peer_urls;
	}();

	return Announce_response{std::move(peer_urls),interval_time,leechers_count,seeds_count};
}

[[nodiscard]]
Udp_torrent_client::scrape_optional Udp_torrent_client::extract_scrape_response(const QByteArray & response,const std::uint32_t sent_txn_id){
	
	if(!verify_txn_id(response,sent_txn_id)){
		return {};
	}

	const auto seeds_count = [&response]{
		constexpr auto seeds_count_offset = 8;
		return util::extract_integer<std::uint32_t>(response,seeds_count_offset);
	}();

	const auto completed_count = [&response]{
		constexpr auto download_count_offset = 12;
		return util::extract_integer<std::uint32_t>(response,download_count_offset);
	}();

	const auto leechers_count = [&response]{
		constexpr auto leechers_count_offset = 16;
		return util::extract_integer<std::uint32_t>(response,leechers_count_offset);
	}();

	return Swarm_metadata{seeds_count,completed_count,leechers_count};
}

[[nodiscard]]
Udp_torrent_client::error_optional Udp_torrent_client::extract_tracker_error(const QByteArray & response,const std::uint32_t sent_txn_id){

	if(!verify_txn_id(response,sent_txn_id)){
		return {};
	}

	constexpr auto error_offset = 8;
	return response.sliced(error_offset);
}