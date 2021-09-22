#ifndef UDP_TORRENT_CLIENT_HXX
#define UDP_TORRENT_CLIENT_HXX

#include "utility.hxx"

#include <QLittleEndianStorageType>
#include <QObject>
#include <QDebug>
#include <memory>
#include <random>

class Udp_torrent_client : public QObject, public std::enable_shared_from_this<Udp_torrent_client> {
	Q_OBJECT
public:
	explicit Udp_torrent_client(bencode::Metadata torrent_metadata) : metadata_(std::move(torrent_metadata)){}

	auto run() noexcept;
signals:
	void stop() const;
private:
	auto craft_connect_request() noexcept;
	auto send_announce_request() noexcept;
	///
	inline static std::mt19937 random_generator = std::mt19937(std::random_device{}());
	inline static std::uniform_int_distribution<std::uint32_t> random_id_range;
	bencode::Metadata metadata_;
};

inline auto Udp_torrent_client::craft_connect_request() noexcept {
	constexpr auto id_size = 4;
	constexpr auto action_size = 4;
	constexpr auto protocol_constant_size = 8;
	constexpr auto protocol_constant = 0x41727101980;

	const static auto protocol_constant_hex = util::conversion::convert_to_hex_array(protocol_constant,protocol_constant_size);
	const static auto action_hex = util::conversion::convert_to_hex_array(0,action_size);
	const auto random_id_hex = util::conversion::convert_to_hex_array(random_id_range(random_generator),id_size);

	const auto message = protocol_constant_hex + action_hex + random_id_hex;

	[[maybe_unused]] constexpr auto required_packet_size = id_size + action_size + protocol_constant_size;
	assert(message.size() == required_packet_size);

	return message;
}

inline auto Udp_torrent_client::run() noexcept {

	connect(this,&Udp_torrent_client::stop,this,[self = shared_from_this()]{
		assert(self.unique());
	},Qt::SingleShotConnection);

	craft_connect_request();

	return shared_from_this();
}

#endif // UDP_TORRENT_CLIENT_HXX