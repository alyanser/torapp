#include "udp_socket.hxx"

void Udp_socket::configure_default_connections() noexcept {

	connect(this,&Udp_socket::connected,[this]{
		send_initial_request(connect_request_,State::Connect);
		connection_timer_.start(get_timeout());
	});
	
	connect(this,&Udp_socket::readyRead,[&connection_timer_ = connection_timer_]{
		connection_timer_.stop();
	});

	connect(&connection_timer_,&QTimer::timeout,[this]{
		constexpr auto protocol_max_factor_limit = 8;

		if(++timeout_factor_ <= protocol_max_factor_limit){
			switch(state_){
				case State::Connect : {
					send_request(connect_request_);
					break;
				}

				case State::Scrape : { 
					send_request(scrape_request_);
					break;
				}

				case State::Announce : {
					send_request(announce_request_);
					break;
				}

				default : __builtin_unreachable();
			}

			connection_timer_.start(get_timeout());
		}else{
			//todo alert the tracker about connection timeout
			connection_timer_.stop();
			disconnectFromHost();
		}
	});
}

void Udp_socket::write_packet(const QByteArray & packet) noexcept {
	assert(packet.size() >= 16);
	write(packet.data(),packet.size());

	constexpr auto txn_id_offset = 12;
	constexpr auto txn_id_bytes = 4;
	constexpr auto hex_base = 16;

	bool conversion_success = true;
	const auto sent_txn_id = packet.sliced(txn_id_offset,txn_id_bytes).toHex().toUInt(&conversion_success,hex_base);
	assert(conversion_success);
	set_txn_id(sent_txn_id);
}