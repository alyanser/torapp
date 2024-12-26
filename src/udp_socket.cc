#include "udp_socket.h"
#include "util.h"

Udp_socket::Udp_socket(const QUrl url, QByteArray connect_request, QObject * const parent) : QUdpSocket(parent), connect_request_(std::move(connect_request)) {
	assert(url.isValid());
	assert(!connect_request_.isEmpty());
	configure_default_connections();
	connectToHost(url.host(), static_cast<std::uint16_t>(url.port()));
}

void Udp_socket::set_requests(QByteArray announce_request, QByteArray scrape_request) noexcept {
	announce_request_ = std::move(announce_request);
	scrape_request_ = std::move(scrape_request);

	connection_id_valid_ = true;

	QTimer::singleShot(std::chrono::minutes(1), this, [&connection_id_valid_ = connection_id_valid_] {
		connection_id_valid_ = false;
	});
}

void Udp_socket::configure_default_connections() noexcept {
	connect(this, &Udp_socket::disconnected, &Udp_socket::deleteLater);
	connect(this, &Udp_socket::readyRead, &connection_timer_, &QTimer::stop);

	connect(this, &Udp_socket::connected, [this] {
		send_initial_request(connect_request_, State::Connect);
	});

	interval_timer_.callOnTimeout(this, [this] {
		send_request(announce_request_);
	});

	connection_timer_.callOnTimeout(this, [this] {
		constexpr auto protocol_max_factor_limit = 8;

		if(++timeout_factor_ <= protocol_max_factor_limit) {

			switch(state_) {

				case State::Connect: {
					send_request(connect_request_);
					break;
				}

				case State::Scrape: {
					send_request(scrape_request_);
					break;
				}

				case State::Announce: {
					send_request(announce_request_);
					break;
				}
			}

			connection_timer_.start(get_timeout());
		} else {
			connection_timer_.stop();
			emit connection_timed_out();
			abort();
		}
	});
}

void Udp_socket::send_packet(const QByteArray & hex_packet) noexcept {
	assert(!hex_packet.isEmpty());

	const auto raw_packet = QByteArray::fromHex(hex_packet);
	write(raw_packet);

	constexpr auto txn_id_offset = 12;
	assert(raw_packet.size() >= txn_id_offset + static_cast<qsizetype>(sizeof(std::int32_t)));
	txn_id_ = util::extract_integer<std::int32_t>(raw_packet, txn_id_offset);
}

void Udp_socket::send_initial_request(const QByteArray & request, const State state) noexcept {
	state_ = state;

	if(state_ != State::Connect && !connection_id_valid_) {
		send_initial_request(connect_request_, State::Connect);
	} else {
		reset_time_specs();
		send_packet(request);
	}
}