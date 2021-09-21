#ifndef UDP_TORRENT_CLIENT_HXX
#define UDP_TORRENT_CLIENT_HXX

#include <bencode_parser.hxx>
#include <QObject>
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
	void send_connect_request() noexcept;
	void send_announce_request() noexcept;
	///
	constexpr static auto protocol_constant = 0x41727101980;
	inline static std::mt19937 random_generator = std::mt19937(std::random_device{}());
	inline static std::uniform_int_distribution<std::uint32_t> random_id_range;
	bencode::Metadata metadata_;
};

inline auto Udp_torrent_client::run() noexcept {

	const auto self_lifetime_connection = connect(this,&Udp_torrent_client::stop,this,[self = shared_from_this()]{
		assert(self.unique());
	},Qt::SingleShotConnection);

	return shared_from_this();
}

inline void Udp_torrent_client::send_connect_request() noexcept {
}

#endif // UDP_TORRENT_CLIENT_HXX