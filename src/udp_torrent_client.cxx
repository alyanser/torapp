#include "udp_torrent_client.hxx"

#include <QBigEndianStorageType>
#include <QNetworkDatagram>
#include <QHostAddress>
#include <QTimer>
#include <QHash>

inline void Udp_torrent_client::send_packet(Udp_socket & socket,const QByteArray & packet) noexcept {
	socket.write(packet.data(),packet.size());

	constexpr auto txn_id_offset = 12;
	constexpr auto txn_id_bytes = 4;

	bool conversion_success = true;
	const auto sent_txn_id = packet.sliced(txn_id_offset,txn_id_bytes).toHex().toUInt(&conversion_success,hex_base);
	assert(conversion_success);

	socket.set_txn_id(sent_txn_id);
	assert(socket.txn_id() == sent_txn_id);
}

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

	const auto connect_request = craft_connect_request();

	for(const auto & announce_url : metadata_.announce_url_list){
		auto socket = std::make_shared<Udp_socket>();
		auto timer = std::make_shared<QTimer>();

		{
			const QUrl url(announce_url.data());
			socket->connectToHost(url.host(),static_cast<std::uint16_t>(url.port()));
		}

		connect(timer.get(),&QTimer::timeout,[this,timer = timer.get(),socket = socket.get(),connect_request]{
			send_packet(*socket,connect_request);

			const auto timeout_limit_reached = [&timeout_factor_ = timeout_factor_]{
				constexpr auto max_protocol_timeout_factor = 8;
				return timeout_factor_ == max_protocol_timeout_factor;
			}();
			
			if(!timeout_limit_reached){
				constexpr auto protocol_delta = 15;
				timer->start(static_cast<std::int32_t>(protocol_delta * std::exp2(++timeout_factor_)));
			}
		});

		connect(socket.get(),&QUdpSocket::connected,this,[socket = socket.get(),connect_request]{
			send_packet(*socket,connect_request);
		});

		const auto read_connection = connect(socket.get(),&QUdpSocket::readyRead,this,[this,socket,connect_request]{
			timeout_factor_ = 0;
			on_socket_ready_read(socket.get());
		});

		connect(socket.get(),&QUdpSocket::disconnected,this,[socket,read_connection]{
			{
				const auto disconnect_success = disconnect(read_connection);
				assert(disconnect_success);
			}

			assert(socket.unique());
			qInfo() << "socket disconected";
		},Qt::SingleShotConnection);
	}
}

void Udp_torrent_client::on_socket_ready_read(Udp_socket * const socket) noexcept {

	auto on_server_action_connect = [this,socket](const QByteArray & tracker_response){

		if(const auto connection_id_opt = verify_connect_response(tracker_response,socket->txn_id())){
			const auto connection_id = connection_id_opt.value();
			const auto announce_request = craft_announce_request(connection_id);

			send_packet(*socket,announce_request);
		}
	};

	auto on_server_action_announce = [socket](const QByteArray & response){

		if(const auto peer_urls = verify_announce_response(response,socket->txn_id());!peer_urls.empty()){
			//todo emit it to thet peer protocol
		}
	};

	auto on_server_action_scrape = []( [[maybe_unused]] const QByteArray & response){
	};

	auto on_tracker_action_error = [socket](const QByteArray & response){
		{
			constexpr auto txn_id_offset = 4;
			constexpr auto txn_id_bytes = 4;

			const auto txn_id = response.sliced(txn_id_offset,txn_id_bytes).toHex().toUInt(nullptr,hex_base);

			if(socket->txn_id() != txn_id){
				// verify this;
				return;
			}
		}

		constexpr auto error_offset = 8;
		const QByteArray tracker_error = response.sliced(error_offset);
		//todo report it to tracker
	};

	while(socket->hasPendingDatagrams()){
		const auto tracker_response = socket->receiveDatagram().data();

		const auto server_action = [&tracker_response]{
			constexpr auto action_offset = 0;
			constexpr auto action_bytes = 4;

			return static_cast<Action_Code>(tracker_response.sliced(action_offset,action_bytes).toHex().toUInt(nullptr,hex_base));
		}();

		assert(static_cast<std::int32_t>(server_action) < 4);

		switch(server_action){
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
		constexpr auto txn_id_bytes = 4;

		const auto response_txn_id = response.sliced(response_txn_offset,txn_id_bytes).toHex().toUInt(nullptr,hex_base);

		if(txn_id_sent != response_txn_id){
			return {};
		}
	}
	
	constexpr auto connection_id_offset = 8;
	constexpr auto connection_id_bytes = 8;

	return quint64_be(response.sliced(connection_id_offset,connection_id_bytes).toHex().toULongLong(nullptr,hex_base));
}

[[nodiscard]]
std::vector<QUrl> Udp_torrent_client::verify_announce_response(const QByteArray & response,const std::uint32_t txn_id_sent) noexcept {

	auto convert_to_hex = [&response](const auto offset,const auto bytes){
		assert(offset + bytes <= response.size());
		return response.sliced(offset,bytes).toHex();
	};

	{
		constexpr auto txn_id_offset = 4;
		constexpr auto txn_id_bytes = 4;

		const auto received_txn_id = convert_to_hex(txn_id_offset,txn_id_bytes).toUInt(nullptr,hex_base);

		if(txn_id_sent != received_txn_id){
			return {};
		}
	}

	//todo report this stuff to the tracker

	{
		constexpr auto interval_offset = 8;
		constexpr auto interval_bytes = 4;

		//! should be unsigned?
		[[maybe_unused]] const auto interval_seconds = convert_to_hex(interval_offset,interval_bytes).toUInt(nullptr,hex_base);
	}
	
	{
		constexpr auto leechers_offset = 12;
		constexpr auto leechers_bytes = 4;

		//! should be unsigned?
		[[maybe_unused]] const auto leechers_count = convert_to_hex(leechers_offset,leechers_bytes).toUInt(nullptr,hex_base);
	}

	{
		constexpr auto seeders_offset = 16;
		constexpr auto seeders_bytes = 4;

		//! should be unsigned?
		[[maybe_unused]] const auto seeders_count = convert_to_hex(seeders_offset,seeders_bytes).toUInt(nullptr,hex_base);
	}

	std::vector<QUrl> peers_urls;

	constexpr auto peers_ip_offset = 20;
	constexpr auto peer_url_bytes = 6;

	for(std::ptrdiff_t i = peers_ip_offset;i < response.size();i += peer_url_bytes){
		constexpr auto ip_bytes = 4;
		constexpr auto port_bytes = 2;

		const auto peer_ip = convert_to_hex(i,ip_bytes).toUInt(nullptr,hex_base);
		const auto peer_port = convert_to_hex(i + ip_bytes,port_bytes).toUShort(nullptr,hex_base);

		auto & url = peers_urls.emplace_back(QHostAddress(peer_ip).toString());
		url.setPort(peer_port);
	}

	return peers_urls;
}