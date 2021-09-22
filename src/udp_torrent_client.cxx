#include "udp_torrent_client.hxx"

#include <QBigEndianStorageType>
#include <QNetworkDatagram>
#include <QUdpSocket>
#include <random>

inline QByteArray Udp_torrent_client::craft_connect_request() noexcept {
	static std::mt19937 random_generator = std::mt19937(std::random_device{}());
	static std::uniform_int_distribution<std::uint32_t> random_id_range;

	const quint32_be transaction_id(random_id_range(random_generator));

	constexpr quint32_be connect_action_code(static_cast<std::uint32_t>(Action_Code::Connect));
	constexpr quint64_be protocol_constant(0x41727101980);

	const static auto protocol_constant_hex = util::conversion::convert_to_hex_array(protocol_constant,Protocol_Constant_Bytes);
	const static auto action_hex = util::conversion::convert_to_hex_array(connect_action_code,Action_Code_Bytes);
	const auto transaction_id_hex = util::conversion::convert_to_hex_array(transaction_id,Transaction_Id_Bytes);

	const auto message = protocol_constant_hex + action_hex + transaction_id_hex;

	constexpr auto total_request_bytes = Protocol_Constant_Bytes + Action_Code_Bytes + Transaction_Id_Bytes;

	assert(message.size() == total_request_bytes);

	return message;
}

void Udp_torrent_client::send_connect_requests() noexcept {

	if(metadata_.announce_url_list.empty()){
		metadata_.announce_url_list.emplace_back(metadata_.announce_url);
	}

	//todo emit error when announce_url_list is empty to the tracker

	const auto connect_request = craft_connect_request();

	for(const auto & announce_url : metadata_.announce_url_list){
		const QUrl url(announce_url.data());
		auto socket = std::make_shared<QUdpSocket>();

		socket->connectToHost(url.host(),static_cast<std::uint16_t>(url.port()));

		auto on_socket_connected = [socket = socket.get(),connect_request = connect_request]{
			const auto connect_request_size = connect_request.size();
			assert(connect_request_size == 16);
			socket->write(connect_request.data(),connect_request_size);
		};

		auto on_socket_ready_read = [socket = socket.get(),connect_request]{
			
			while(socket->hasPendingDatagrams()){
				const auto datagram = socket->receiveDatagram();

				if(datagram.data().size() == 16){ // connect response

					if(const auto connection_id_opt =  verify_connect_response(connect_request,datagram.data())){
						const auto connection_id = connection_id_opt.value();
					}

				}else{ // announce response
					//todo extract the peer ip and send back request to network manager
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

std::optional<std::uint64_t> Udp_torrent_client::verify_connect_response(const QByteArray & request,const QByteArray & response) noexcept {
	assert(request.size() == 16 && response.size() == 16);

	{	// verify integrity of transaction id

		constexpr auto request_transaction_begin_index = 12;
		constexpr auto response_transaction_begin_index = 4;

		const auto request_transaction_id = request.sliced(request_transaction_begin_index,Transaction_Id_Bytes);
		const auto response_transaction_id = response.sliced(response_transaction_begin_index,Transaction_Id_Bytes);

		if(request_transaction_id != response_transaction_id){
			return {};
		}
	}
	
	{	// verify action code

		constexpr auto request_action_code_begin_index = 8;
		constexpr auto response_action_code_begin_index = 0;

		const auto request_action_code = request.sliced(request_action_code_begin_index,Action_Code_Bytes);
		const auto response_action_code = response.sliced(response_action_code_begin_index,Action_Code_Bytes);

		assert(request_action_code.size() == Action_Code_Bytes && response_action_code.size() == Action_Code_Bytes);

		//? operator '==' stops at null termination. find an alt later
		for(const auto c : request_action_code){
			assert(c == '\x00');
		}

		if(request_action_code != response_action_code){
			return {};
		}
	}

	constexpr auto connection_id_begin_index = 8;
	constexpr auto connection_id_bytes = 8;

	const auto connection_id = response.sliced(connection_id_begin_index,connection_id_bytes);

	qInfo() << connection_id;

	return {};
}