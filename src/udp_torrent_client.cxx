#include "udp_torrent_client.hxx"

#include <QBigEndianStorageType>
#include <QNetworkDatagram>
#include <QHostAddress>
#include <QTimer>

inline QByteArray Udp_torrent_client::craft_connect_request() noexcept {
	QByteArray connect_request;

	{
		constexpr quint64_be protocol_constant(0x41727101980);
		connect_request = util::conversion::convert_to_hex(protocol_constant,Protocol_Constant_Bytes);
	}

	{
		constexpr quint32_be connect_action_code(static_cast<std::uint32_t>(Action_Code::Connect));
		connect_request += util::conversion::convert_to_hex(connect_action_code,Action_Code_Bytes);
	}

	{
		const quint32_be transaction_id(random_id_range(random_generator));
		connect_request += util::conversion::convert_to_hex(transaction_id,Transaction_Id_Bytes);
	}

	[[maybe_unused]] constexpr auto total_request_bytes = Protocol_Constant_Bytes + Action_Code_Bytes + Transaction_Id_Bytes;

	assert(connect_request.size() == total_request_bytes);

	return connect_request;
}

QByteArray Udp_torrent_client::craft_announce_request(const std::uint64_t server_connection_id) const noexcept {
	QByteArray announce_request = util::conversion::convert_to_hex(server_connection_id,sizeof(quint64_be));

	{
		constexpr quint32_be connect_action_code(static_cast<std::uint32_t>(Action_Code::Announce));
		announce_request += util::conversion::convert_to_hex(connect_action_code,Action_Code_Bytes);
	}

	{
		const quint32_be transaction_id(random_id_range(random_generator));
		announce_request += util::conversion::convert_to_hex(transaction_id,Transaction_Id_Bytes);
	}

	announce_request += QByteArray(metadata_.pieces.data());
	announce_request += QByteArray::fromHex(peer_id);
	announce_request += util::conversion::convert_to_hex(downloaded_,8);
	announce_request += util::conversion::convert_to_hex(left_,8);
	announce_request += util::conversion::convert_to_hex(uploaded_,8);
	announce_request += util::conversion::convert_to_hex(static_cast<std::uint32_t>(event_),4);

	{
		constexpr auto default_ip_address = 0;
		announce_request += util::conversion::convert_to_hex(default_ip_address,4);
	}

	{
		const quint32_be random_key(random_id_range(random_generator));
		announce_request += util::conversion::convert_to_hex(random_key,4);
	}

	{
		constexpr qint32_be default_num_want(-1);
		announce_request += util::conversion::convert_to_hex(default_num_want,4);
	}

	{
		constexpr quint32_be default_port(6889);
		announce_request += util::conversion::convert_to_hex(default_port,4);
	}

	return announce_request;
}

void Udp_torrent_client::send_connect_requests() noexcept {

	if(metadata_.announce_url_list.empty()){
		metadata_.announce_url_list.emplace_back(metadata_.announce_url);
	}

	for(const auto & announce_url : metadata_.announce_url_list){
		const auto connect_request = craft_connect_request();
		auto socket = std::make_shared<QUdpSocket>();
		auto timeout_timer = std::make_shared<QTimer>();

		{
			const QUrl url(announce_url.data());
			socket->connectToHost(url.host(),static_cast<std::uint16_t>(url.port()));
		}

		connect(timeout_timer.get(),&QTimer::timeout,[socket = socket.get(),connect_request = connect_request]{
			send_packet(*socket,connect_request.data(),connect_request.size());
		});

		auto on_socket_connected = [socket = socket.get(),connect_request = connect_request]{
			send_packet(*socket,connect_request.data(),connect_request.size());

			[[maybe_unused]] const auto connect_request_size = connect_request.size();
			assert(connect_request_size == 16);
		};

		auto on_socket_ready_read = [this,socket = socket.get(),connect_request]{
			
			while(socket->hasPendingDatagrams()){
				const auto datagram = socket->receiveDatagram();
				const auto response = datagram.data();

				if(response.size() >= 20){ // announce response

					if(const auto peer_urls = verify_announce_response(datagram.data());!peer_urls.empty()){
						//todo emit it to thet peer protocol
					}

				}else if(response.size() == 16){ // connect response

					auto extract_transaction_id = [](auto && announce_request){
						constexpr auto hex_base = 16;
						return announce_request.sliced(12,Transaction_Id_Bytes).toHex().toUInt(nullptr,hex_base);
					};

					if(const auto connection_id_opt = verify_connect_response(connect_request,datagram.data())){
						const auto connection_id = connection_id_opt.value();
						const auto announce_request = craft_announce_request(connection_id);
						//! hacky fix. reconsider the approach
						const auto transaction_id = extract_transaction_id(announce_request);
						announced_random_ids_.insert(transaction_id);

						connect(socket,&QUdpSocket::disconnected,[this,transaction_id]{
							announced_random_ids_.remove(transaction_id);
						});
						
						send_packet(*socket,announce_request.data(),announce_request.size());
					}
				}
			}
		};

		connect(socket.get(),&QUdpSocket::connected,this,on_socket_connected);
		connect(socket.get(),&QUdpSocket::readyRead,this,on_socket_ready_read);

		connect(socket.get(),&QUdpSocket::disconnected,this,[socket]{
			//? consider using delete later here
			assert(socket.unique());
		},Qt::SingleShotConnection);
	}
}

std::optional<quint64_be> Udp_torrent_client::verify_connect_response(const QByteArray & request,const QByteArray & response) noexcept {
	{
	 	[[maybe_unused]] constexpr auto expected_packet_size = 16;
		assert(request.size() == expected_packet_size && response.size() >= expected_packet_size);
	}

	{
		constexpr auto request_transaction_begin_index = 12;
		constexpr auto response_transaction_begin_index = 4;

		const auto request_transaction_id = request.sliced(request_transaction_begin_index,Transaction_Id_Bytes).toHex();
		const auto response_transaction_id = response.sliced(response_transaction_begin_index,Transaction_Id_Bytes).toHex();

		if(request_transaction_id != response_transaction_id){
			return {};
		}
	}
	
	{	
		//todo verbose consider shorter names
		constexpr auto request_action_code_begin_index = 8;
		constexpr auto response_action_code_begin_index = 0;

		const auto request_action_code = request.sliced(request_action_code_begin_index,Action_Code_Bytes).toHex();
		const auto response_action_code = response.sliced(response_action_code_begin_index,Action_Code_Bytes).toHex();

		constexpr auto action_code_hex_bytes = Action_Code_Bytes * 2;

		assert(request_action_code.size() == action_code_hex_bytes && response_action_code.size() == action_code_hex_bytes);

		{
			[[maybe_unused]] constexpr std::string_view connect_action_code_hex = "00000000";
			assert(request_action_code == connect_action_code_hex.data());
		}
			
		if(request_action_code != response_action_code){
			return {};
		}
	}

	constexpr auto connection_id_begin_index = 8;
	constexpr auto connection_id_bytes = 8;
	constexpr auto hex_base = 16;

	const auto connection_id = response.sliced(connection_id_begin_index,connection_id_bytes).toHex();

	//? do we check if the conversion succeeds
	return quint64_be(connection_id.toULongLong(nullptr,hex_base));
}

std::vector<QUrl> Udp_torrent_client::verify_announce_response(const QByteArray & response) const noexcept {
	constexpr auto hex_base = 16;

	auto convert_to_hex = [&response](const auto begin_index,const auto bytes){
		assert(begin_index + bytes <= response.size());
		return response.sliced(begin_index,bytes).toHex();
	};

	//todo maybe consider adding checks for conversions

	{
		constexpr auto action_code_begin_index = 0;
		const auto received_action_code = convert_to_hex(action_code_begin_index,Action_Code_Bytes).toUInt(nullptr,hex_base);

		if(received_action_code != static_cast<std::uint32_t>(Action_Code::Announce)){
			return {};
		}
	}

	{
		constexpr auto transaction_id_begin_index = 4;
		const auto received_transaction_id = convert_to_hex(transaction_id_begin_index,Transaction_Id_Bytes).toUInt(nullptr,hex_base);

		if(!announced_random_ids_.contains(received_transaction_id)){
			return {};
		}
	}

	//todo report this stuff to the tracker

	{
		constexpr auto interval_begin_index = 8;
		constexpr auto interval_bytes = 4;

		//! should be unsigned?
		const auto interval_seconds = convert_to_hex(interval_begin_index,interval_bytes).toUInt(nullptr,hex_base);
	}
	
	{
		constexpr auto leechers_begin_index = 12;
		constexpr auto leechers_bytes = 4;

		//! should be unsigned?
		const auto leechers_count = convert_to_hex(leechers_begin_index,leechers_bytes).toUInt(nullptr,hex_base);
	}

	{
		constexpr auto seeders_begin_index = 16;
		constexpr auto seeders_bytes = 4;

		//! should be unsigned?
		const auto seeders_count = convert_to_hex(seeders_begin_index,seeders_bytes).toUInt(nullptr,hex_base);
	}

	std::vector<QUrl> peers_urls;

	constexpr auto peers_ip_begin_index = 20;
	constexpr auto peer_url_bytes = 6;

	for(std::ptrdiff_t i = peers_ip_begin_index;i < response.size();i += peer_url_bytes){
		constexpr auto ip_bytes = 4;
		constexpr auto port_bytes = 2;

		const auto peer_ip = convert_to_hex(i,ip_bytes).toUInt(nullptr,hex_base);
		const auto peer_port = convert_to_hex(i + ip_bytes,port_bytes).toUShort(nullptr,hex_base);

		auto & url = peers_urls.emplace_back(QHostAddress(peer_ip).toString());
		url.setPort(peer_port);
	}

	return peers_urls;
}