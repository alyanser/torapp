#ifndef UDP_SOCKET_HXX
#define UDP_SOCKET_HXX

#include <QUdpSocket>
#include <QByteArray>
#include <QTimer>
#include <QUrl>

class Udp_socket : public QUdpSocket, public std::enable_shared_from_this<Udp_socket> {
	Q_OBJECT

	enum class State { 
		Connect,
		Scrape,
		Announce 
	};

	Q_ENUM(State);

public :
	explicit Udp_socket(const QUrl & url);

	std::shared_ptr<Udp_socket> bind_lifetime() noexcept;
	constexpr std::uint32_t txn_id() const noexcept;

	constexpr void set_interval_time(std::chrono::seconds interval_time) noexcept;
	constexpr std::chrono::seconds interval_time() const noexcept;

	void set_connect_request(QByteArray connect_request) noexcept;
	void set_announce_request(QByteArray announce_request) noexcept;
	
	const QByteArray & connect_request() const noexcept;
	const QByteArray & announce_request() const noexcept;

	void send_initial_connect_request() noexcept;
	void send_initial_announce_request() noexcept;
private:
	constexpr void set_txn_id(std::uint32_t txn_id) noexcept;
	void configure_default_connections() noexcept;
	std::int32_t get_timeout_seconds() const noexcept;
	void send_packet(const QByteArray & packet) noexcept;
	void send_announce_request() noexcept;
	void send_connect_request() noexcept;
	///
	QByteArray connect_request_;
	QByteArray announce_request_;
	QTimer connection_timer_;
	QTimer validity_timer_;
	std::chrono::seconds interval_time_ {};
	State state_ = State::Connect;
	std::uint32_t txn_id_ = 0;
	std::uint8_t timeout_factor_ = 0;
};

inline Udp_socket::Udp_socket(const QUrl & url){

	configure_default_connections();
	connectToHost(url.host(),static_cast<std::uint16_t>(url.port()));
}

inline void Udp_socket::send_initial_connect_request() noexcept {
	state_ = State::Connect;
	timeout_factor_ = 0;
	connection_timer_.start(get_timeout_seconds());
	
	send_connect_request();
}

inline void Udp_socket::send_initial_announce_request() noexcept {
	state_ = State::Announce;
	timeout_factor_ = 0;
	connection_timer_.start(get_timeout_seconds());

	send_announce_request();
}

inline std::shared_ptr<Udp_socket> Udp_socket::bind_lifetime() noexcept {

	connect(this,&Udp_socket::disconnected,this,[self = shared_from_this()]{
		assert(self.unique());
	},Qt::SingleShotConnection);

	return shared_from_this();
}

inline void Udp_socket::configure_default_connections() noexcept {

	connect(this,&Udp_socket::connected,this,&Udp_socket::send_initial_connect_request);
	
	connect(this,&Udp_socket::readyRead,[&connection_timer_ = connection_timer_]{
		connection_timer_.stop();
	});

	connect(&connection_timer_,&QTimer::timeout,[this]{
		constexpr auto protocol_max_limit = 8;

		if(++timeout_factor_ <= protocol_max_limit){
			switch(state_){

				case State::Connect : {
					send_connect_request();
					break;
				}

				case State::Scrape : { 
					//todo add scrape
					break;
				}

				case State::Announce : {
					send_announce_request();
					break;
				}

				default : __builtin_unreachable();
			}
			
			connection_timer_.start(get_timeout_seconds());
		}else{
			//todo alert the tracker about connection timeout
			connection_timer_.stop();
			disconnectFromHost();
		}
	});
}

constexpr void Udp_socket::set_txn_id(std::uint32_t txn_id) noexcept {
	txn_id_ = txn_id;
}

[[nodiscard]]
constexpr std::uint32_t Udp_socket::txn_id() const noexcept {
	return txn_id_;
}

constexpr void Udp_socket::set_interval_time(std::chrono::seconds interval_time) noexcept {
	interval_time_ = interval_time;
}

constexpr std::chrono::seconds Udp_socket::interval_time() const noexcept {
	return interval_time_;
}

inline void Udp_socket::set_connect_request(QByteArray connect_request) noexcept {
	connect_request_ = std::move(connect_request);
}

[[nodiscard]]
inline const QByteArray & Udp_socket::connect_request() const noexcept {
	return connect_request_;
}

[[nodiscard]]
inline const QByteArray & Udp_socket::announce_request() const noexcept {
	return announce_request_;
}

inline void Udp_socket::send_connect_request() noexcept {
	assert(!connect_request_.isEmpty());
	send_packet(connect_request_);
}

inline void Udp_socket::send_announce_request() noexcept {
	assert(!announce_request_.isEmpty());
	send_packet(announce_request_);
}

inline void Udp_socket::set_announce_request(QByteArray announce_request) noexcept {
	announce_request_ = std::move(announce_request);
}

[[nodiscard]]
inline std::int32_t Udp_socket::get_timeout_seconds() const noexcept {
	constexpr auto protocol_delta = 15;
	const auto timeout_seconds = protocol_delta * static_cast<std::int32_t>(std::exp2(timeout_factor_));

	[[maybe_unused]] constexpr auto max_timeout_seconds = 3840;
	assert(timeout_seconds <= max_timeout_seconds);

	return timeout_seconds;
}

inline void Udp_socket::send_packet(const QByteArray & packet) noexcept {
	write(packet.data(),packet.size());

	constexpr auto txn_id_offset = 12;
	constexpr auto txn_id_bytes = 4;
	constexpr auto hex_base = 16;

	bool conversion_success = true;
	const auto sent_txn_id = packet.sliced(txn_id_offset,txn_id_bytes).toHex().toUInt(&conversion_success,hex_base);
	assert(conversion_success);
	set_txn_id(sent_txn_id);
}

#endif // UDP_SOCKET_HXX