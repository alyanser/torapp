#include "udp_torrent_client.hxx"
#include "peer_wire_client.hxx"

#include <QBigEndianStorageType>
#include <QNetworkDatagram>

void Udp_torrent_client::configure_default_connections() const noexcept {

	connect(this,&Udp_torrent_client::announce_response_received,[this](const Announce_response & announce_response){
		auto peer_client = std::make_shared<Peer_wire_client>(peer_id,info_sha1_hash_)->bind_lifetime();
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
			on_socket_ready_read(socket);
		});
	}
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_connect_request() noexcept {

	QByteArray connect_request = []{
		constexpr quint64_be protocol_constant(0x41727101980);
		const auto store = util::conversion::convert_to_hex(protocol_constant,sizeof(protocol_constant));

		return store;
	}();

	connect_request += []{
		constexpr quint32_be connect_action(static_cast<std::uint32_t>(Action_Code::Connect));
		return util::conversion::convert_to_hex(connect_action,sizeof(connect_action));
	}();

	connect_request += []{
		const quint32_be txn_id(random_id_range(random_generator));
		return util::conversion::convert_to_hex(txn_id,sizeof(txn_id));
	}();

	return connect_request;
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_announce_request(const quint64_be tracker_connection_id) const noexcept {
	QByteArray announce_request = util::conversion::convert_to_hex(tracker_connection_id,sizeof(quint64_be));

	announce_request += []{
		constexpr quint32_be connect_action(static_cast<std::uint32_t>(Action_Code::Announce));
		return util::conversion::convert_to_hex(connect_action,sizeof(connect_action));
	}();

	announce_request += []{
		const quint32_be txn_id(random_id_range(random_generator));
		return util::conversion::convert_to_hex(txn_id,sizeof(txn_id));
	}();

	announce_request += info_sha1_hash_;
	announce_request += peer_id;
	announce_request += util::conversion::convert_to_hex(downloaded_,sizeof(downloaded_));
	announce_request += util::conversion::convert_to_hex(left_,sizeof(left_));
	announce_request += util::conversion::convert_to_hex(uploaded_,sizeof(uploaded_));
	announce_request += util::conversion::convert_to_hex(static_cast<std::uint32_t>(event_),sizeof(event_));

	announce_request += []{
		constexpr auto default_ip_address = 0;
		return util::conversion::convert_to_hex(default_ip_address,sizeof(default_ip_address));
	}();

	announce_request += []{
		const quint32_be random_key(random_id_range(random_generator));
		return util::conversion::convert_to_hex(random_key,sizeof(random_key));
	}();

	announce_request += []{
		constexpr qint32_be default_num_want(-1);
		return util::conversion::convert_to_hex(default_num_want,sizeof(default_num_want));
	}();

	announce_request += []{
		constexpr quint32_be default_port(6889);
		return util::conversion::convert_to_hex(default_port,sizeof(default_port));
	}();

	return announce_request;
}

QByteArray Udp_torrent_client::craft_scrape_request(const bencode::Metadata & metadata,const quint64_be tracker_connection_id) noexcept {
	QByteArray scrape_request = util::conversion::convert_to_hex(tracker_connection_id,sizeof(tracker_connection_id));

	scrape_request += []{
		constexpr quint32_be scrape_action(static_cast<std::uint32_t>(Action_Code::Scrape));
		return util::conversion::convert_to_hex(scrape_action,sizeof(scrape_action));
	}();

	scrape_request += []{
		const quint32_be txn_id(random_id_range(random_generator));
		return util::conversion::convert_to_hex(txn_id,sizeof(txn_id));
	}();

	return scrape_request + QByteArray(metadata.pieces.data());
}

[[nodiscard]]
bool Udp_torrent_client::verify_txn_id(const QByteArray & response,std::uint32_t sent_txn_id) noexcept {
	assert(response.size() >= 8);
	
	constexpr auto txn_id_offset = 4;
	constexpr auto txn_id_bytes = 4;
		
	const auto received_txn_id = response.sliced(txn_id_offset,txn_id_bytes).toHex().toUInt(nullptr,hex_base);
	
	return sent_txn_id == received_txn_id;
}

void Udp_torrent_client::on_socket_ready_read(Udp_socket * const socket) noexcept {

	auto on_tracker_action_connect = [this,socket](const QByteArray & tracker_response){

		if(const auto connection_id_opt = extract_connect_response(tracker_response,socket->txn_id())){
			const auto connection_id = connection_id_opt.value();

			socket->set_announce_request(craft_announce_request(connection_id));
			socket->set_scrape_request(craft_scrape_request(torrent_metadata_,connection_id));

			socket->send_initial_request(socket->announce_request(),Udp_socket::State::Announce);
		}
	};

	auto on_tracker_action_announce = [this,socket](const QByteArray & response){

		if(const auto announce_response_opt = extract_announce_response(response,socket->txn_id())){
			emit announce_response_received(announce_response_opt.value());
		}
	};

	auto on_tracker_action_scrape = [this,socket](const QByteArray & response){

		if(const auto scrape_response_opt = extract_scrape_response(response,socket->txn_id())){
			emit swarm_metadata_received(scrape_response_opt.value());
		}
	};

	auto on_tracker_action_error = [this,socket](const QByteArray & response){

		if(const auto tracker_error_opt = extract_tracker_error(response,socket->txn_id())){
			emit error_received(tracker_error_opt.value());
		}
	};

	while(socket->hasPendingDatagrams()){
		const auto tracker_response = socket->receiveDatagram().data();

		const auto tracker_action = [&tracker_response]{
			constexpr auto action_offset = 0;
			constexpr auto action_bytes = 4;

			return static_cast<Action_Code>(tracker_response.sliced(action_offset,action_bytes).toHex().toUInt(nullptr,hex_base));
		}();

		switch(tracker_action){
			case Action_Code::Connect : {
				on_tracker_action_connect(tracker_response); 
				break;
			}

			case Action_Code::Announce : {
				on_tracker_action_announce(tracker_response);
				break;
			}

			case Action_Code::Scrape : {
				on_tracker_action_scrape(tracker_response);
				break;
			}

			case Action_Code::Error : {
				on_tracker_action_error(tracker_response);
				break;
			}

			default : {
				qDebug() << "tracker replied with invalid action code" << static_cast<std::uint32_t>(tracker_action);
			}
		}
	}
}

[[nodiscard]]
std::optional<quint64_be> Udp_torrent_client::extract_connect_response(const QByteArray & response,const std::uint32_t sent_txn_id) noexcept {

	if(constexpr auto min_response_size = 12;response.size() < min_response_size || !verify_txn_id(response,sent_txn_id)){
		return {};
	}
	
	constexpr auto connection_id_offset = 8;
	constexpr auto connection_id_bytes = 8;

	return quint64_be(response.sliced(connection_id_offset,connection_id_bytes).toHex().toULongLong(nullptr,hex_base));
}

[[nodiscard]]
Udp_torrent_client::announce_optional Udp_torrent_client::extract_announce_response(const QByteArray & response,const std::uint32_t sent_txn_id) noexcept {
	
	if(constexpr auto min_response_size = 20;response.size() < min_response_size || !verify_txn_id(response,sent_txn_id)){
		return {};
	}

	const auto interval_time = [&response]{
		constexpr auto interval_bytes = 4;
		constexpr auto interval_offset = 8;

		return response.sliced(interval_offset,interval_bytes).toHex().toUInt(nullptr,hex_base);
	}();

	const auto leechers_count = [&response]{
		constexpr auto leechers_offset = 12;
		constexpr auto leechers_bytes = 4;

		return response.sliced(leechers_offset,leechers_bytes).toHex().toUInt(nullptr,hex_base);
	}();

	const auto seeds_count = [&response]{
		constexpr auto seeders_offset = 16;
		constexpr auto seeders_bytes = 4;

		return response.sliced(seeders_offset,seeders_bytes).toHex().toUInt(nullptr,hex_base);
	}();

	auto peer_urls = [&response]{
		std::vector<QUrl> peer_urls;

		constexpr auto peers_ip_offset = 20;
		constexpr auto peer_url_bytes = 6;

		for(std::ptrdiff_t idx = peers_ip_offset;idx < response.size();idx += peer_url_bytes){
			constexpr auto ip_bytes = 4;
			constexpr auto port_bytes = 2;

			const auto peer_ip = response.sliced(idx,ip_bytes).toHex().toUInt(nullptr,hex_base);
			const auto peer_port = response.sliced(idx + ip_bytes,port_bytes).toHex().toUShort(nullptr,hex_base);

			auto & url = peer_urls.emplace_back();
			
			url.setHost(QHostAddress(peer_ip).toString());
			url.setPort(peer_port);
		}

		return peer_urls;
	}();

	return Announce_response{std::move(peer_urls),interval_time,leechers_count,seeds_count};
}

[[nodiscard]]
Udp_torrent_client::scrape_optional Udp_torrent_client::extract_scrape_response(const QByteArray & response,const std::uint32_t sent_txn_id) noexcept {
	
	if(constexpr auto min_response_size = 20;response.size() < min_response_size || !verify_txn_id(response,sent_txn_id)){
		return {};
	}

	const auto seeds_count = [&response]{
		constexpr auto seeds_count_offset = 8;
		constexpr auto seeds_count_bytes =  4;

		return response.sliced(seeds_count_offset,seeds_count_bytes).toHex().toUInt(nullptr,hex_base);
	}();

	const auto completed_count = [&response]{
		constexpr auto download_count_offset = 12;
		constexpr auto download_count_bytes = 4;

		return response.sliced(download_count_offset,download_count_bytes).toHex().toUInt(nullptr,hex_base);
	}();

	const auto leechers_count = [&response]{
		constexpr auto leechers_count_offset = 16;
		constexpr auto leechers_count_bytes =  4;

		return response.sliced(leechers_count_offset,leechers_count_bytes).toHex().toUInt(nullptr,hex_base);
	}();

	return Swarm_metadata{seeds_count,completed_count,leechers_count};
}

[[nodiscard]]
Udp_torrent_client::error_optional Udp_torrent_client::extract_tracker_error(const QByteArray & response,std::uint32_t sent_txn_id) noexcept {

	if(constexpr auto min_response_size = 8;response.size() < min_response_size || !verify_txn_id(response,sent_txn_id)){
		return {};
	}

	constexpr auto error_offset = 8;

	return response.sliced(error_offset);
}