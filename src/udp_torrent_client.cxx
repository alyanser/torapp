#include "udp_torrent_client.hxx"

#include <QBigEndianStorageType>
#include <QNetworkDatagram>
#include <QHostAddress>
#include <QTimer>

[[nodiscard]]
inline QByteArray Udp_torrent_client::craft_connect_request() noexcept {
	QByteArray connect_request;

	{
		constexpr quint64_be protocol_constant(0x41727101980);
		connect_request = util::conversion::convert_to_hex(protocol_constant,sizeof(protocol_constant));
	}

	{
		constexpr quint32_be connect_action(static_cast<std::uint32_t>(Action_Code::Connect));
		connect_request += util::conversion::convert_to_hex(connect_action,sizeof(connect_action));
	}

	{
		const quint32_be txn_id(random_id_range(random_generator));
		connect_request += util::conversion::convert_to_hex(txn_id,sizeof(txn_id));
	}

	[[maybe_unused]] constexpr auto total_request_bytes = 16;
	assert(connect_request.size() == total_request_bytes);

	return connect_request;
}

[[nodiscard]]
QByteArray Udp_torrent_client::craft_announce_request(const std::uint64_t server_connection_id) const noexcept {
	QByteArray announce_request = util::conversion::convert_to_hex(server_connection_id,sizeof(quint64_be));

	{
		constexpr quint32_be connect_action(static_cast<std::uint32_t>(Action_Code::Announce));
		announce_request += util::conversion::convert_to_hex(connect_action,sizeof(connect_action));
	}

	{
		const quint32_be txn_id(random_id_range(random_generator));
		announce_request += util::conversion::convert_to_hex(txn_id,sizeof(txn_id));
	}

	announce_request += QByteArray(metadata_.pieces.data());
	announce_request += QByteArray::fromHex(peer_id);
	announce_request += util::conversion::convert_to_hex(downloaded_,sizeof(downloaded_));
	announce_request += util::conversion::convert_to_hex(left_,sizeof(left_));
	announce_request += util::conversion::convert_to_hex(uploaded_,sizeof(uploaded_));
	announce_request += util::conversion::convert_to_hex(static_cast<std::uint32_t>(event_),sizeof(event_));

	{
		constexpr auto default_ip_address = 0;
		announce_request += util::conversion::convert_to_hex(default_ip_address,sizeof(default_ip_address));
	}

	{
		const quint32_be random_key(random_id_range(random_generator));
		announce_request += util::conversion::convert_to_hex(random_key,sizeof(random_key));
	}

	{
		constexpr qint32_be default_num_want(-1);
		announce_request += util::conversion::convert_to_hex(default_num_want,sizeof(default_num_want));
	}

	{
		constexpr quint32_be default_port(6889);
		announce_request += util::conversion::convert_to_hex(default_port,sizeof(default_port));
	}

	return announce_request;
}

void Udp_torrent_client::send_connect_requests() noexcept {

	if(metadata_.announce_url_list.empty()){
		metadata_.announce_url_list.emplace_back(metadata_.announce_url);
	}

	for(const auto & announce_url : metadata_.announce_url_list){
		auto socket = std::make_shared<Udp_socket>(QUrl(announce_url.data()))->bind_lifetime();

		socket->set_connect_request(craft_connect_request());
		
		connect(socket.get(),&Udp_socket::readyRead,this,[this,socket = socket.get()]{
			on_socket_ready_read(socket);
		});
	}
}

//? consider using weak pointer here
void Udp_torrent_client::on_socket_ready_read(Udp_socket * const socket) noexcept {

	auto on_server_action_connect = [this,socket](const QByteArray & tracker_response){

		if(const auto connection_id_opt = verify_connect_response(tracker_response,socket->txn_id())){
			const auto connection_id = connection_id_opt.value();

			socket->set_announce_request(craft_announce_request(connection_id));
			socket->send_initial_announce_request();
		}
	};

	auto on_server_action_announce = [socket](const QByteArray & response){

		if(const auto peer_urls = verify_announce_response(response,socket);!peer_urls.empty()){
			//todo emit it to thet peer protocol
		}
	};

	auto on_server_action_scrape = []( [[maybe_unused]] const QByteArray & response){
	};

	auto on_tracker_action_error = [socket](const QByteArray & response){

		{
			constexpr auto tracker_txn_id_offset = 4;
			constexpr auto tracker_txn_id_bytes = 4;

			const auto txn_id = response.sliced(tracker_txn_id_offset,tracker_txn_id_bytes).toHex().toUInt(nullptr,hex_base);

			if(socket->txn_id() != txn_id){
				return;
			}
		}

		constexpr auto error_offset = 8;
		const QByteArray tracker_error = response.sliced(error_offset);
		//todo report it to tracker
	};

	while(socket->hasPendingDatagrams()){
		const auto tracker_response = socket->receiveDatagram().data();

		const auto tracker_action = [&tracker_response]{
			constexpr auto action_offset = 0;
			constexpr auto action_bytes = 4;

			const auto tracker_action = tracker_response.sliced(action_offset,action_bytes).toHex().toUInt(nullptr,hex_base);
			return static_cast<Action_Code>(tracker_action);
		}();

		switch(tracker_action){
			case Action_Code::Connect : {
				on_server_action_connect(tracker_response); 
				break;
			}

			case Action_Code::Announce : {
				on_server_action_announce(tracker_response);
				break;
			}

			case Action_Code::Scrape : {
				on_server_action_scrape(tracker_response);
				break;
			}

			case Action_Code::Error : {
				on_tracker_action_error(tracker_response);
				break;
			}
		}
	}
}

[[nodiscard]]
std::optional<quint64_be> Udp_torrent_client::verify_connect_response(const QByteArray & response,const std::uint32_t txn_id_sent) noexcept {

	{
		constexpr auto expected_packet_size = 16;

		if(response.size() != expected_packet_size){
			return {};
		}
	}

	{
		constexpr auto response_txn_offset = 4;
		constexpr auto tracker_txn_id_bytes = 4;

		const auto response_txn_id = response.sliced(response_txn_offset,tracker_txn_id_bytes).toHex().toUInt(nullptr,hex_base);

		if(txn_id_sent != response_txn_id){
			return {};
		}
	}
	
	constexpr auto connection_id_offset = 8;
	constexpr auto connection_id_bytes = 8;

	return quint64_be(response.sliced(connection_id_offset,connection_id_bytes).toHex().toULongLong(nullptr,hex_base));
}

[[nodiscard]]
std::vector<QUrl> Udp_torrent_client::verify_announce_response(const QByteArray & response,Udp_socket * const socket) noexcept {

	auto convert_to_hex = [&response](const auto offset,const auto bytes){
		assert(offset + bytes <= response.size());
		return response.sliced(offset,bytes).toHex();
	};

	{
		constexpr auto tracker_txn_id_offset = 4;
		constexpr auto tracker_txn_id_bytes = 4;

		const auto received_txn_id = convert_to_hex(tracker_txn_id_offset,tracker_txn_id_bytes).toUInt(nullptr,hex_base);

		if(socket->txn_id() != received_txn_id){
			return {};
		}
	}

	//todo report this stuff to the tracker
	//! consider the unsignedness of these segments below

	{
		constexpr auto interval_offset = 8;
		constexpr auto interval_bytes = 4;

		const auto interval_time = convert_to_hex(interval_offset,interval_bytes).toUInt(nullptr,hex_base);

		socket->set_interval_time(std::chrono::seconds(interval_time));
	}
	
	{
		constexpr auto leechers_offset = 12;
		constexpr auto leechers_bytes = 4;

		[[maybe_unused]] const auto leechers_count = convert_to_hex(leechers_offset,leechers_bytes).toUInt(nullptr,hex_base);
	}

	{
		constexpr auto seeders_offset = 16;
		constexpr auto seeders_bytes = 4;

		[[maybe_unused]] const auto seeders_count = convert_to_hex(seeders_offset,seeders_bytes).toUInt(nullptr,hex_base);
	}

	std::vector<QUrl> peer_urls;

	constexpr auto peers_ip_offset = 20;
	constexpr auto peer_url_bytes = 6;

	for(std::ptrdiff_t i = peers_ip_offset;i < response.size();i += peer_url_bytes){
		constexpr auto ip_bytes = 4;
		constexpr auto port_bytes = 2;

		const auto peer_ip = convert_to_hex(i,ip_bytes).toUInt(nullptr,hex_base);
		const auto peer_port = convert_to_hex(i + ip_bytes,port_bytes).toUShort(nullptr,hex_base);

		auto & url = peer_urls.emplace_back(QHostAddress(peer_ip).toString());
		url.setPort(peer_port);
	}

	return peer_urls;
}