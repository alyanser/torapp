#include "udp_socket.hxx"
#include "utility.hxx"

void Udp_socket::configure_default_connections() noexcept {

	connect(this,&Udp_socket::connected,[this]{
		send_initial_request(connect_request_,State::Connect);
	});
	
	connect(this,&Udp_socket::readyRead,[&connection_timer_ = connection_timer_]{
		connection_timer_.stop();
	});

	interval_timer_.callOnTimeout([this]{
		qInfo() << "interval timer timeout out" << interval_timer_.intervalAsDuration().count();
		send_request(announce_request_);
	});

	connection_timer_.callOnTimeout([this]{
		qInfo() << "connection timer timed out";
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

void Udp_socket::send_packet(const QByteArray & packet) noexcept {
	const auto raw_fmt = QByteArray::fromHex(packet);
	
	write(raw_fmt);

	constexpr auto txn_id_offset = 12;
	bool conversion_success = true;
	
	const auto sent_txn_id = util::extract_integer<std::uint32_t>(raw_fmt,txn_id_offset);

	assert(conversion_success);
	set_txn_id(sent_txn_id);
}