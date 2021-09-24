#ifndef UDP_TORRENT_CLIENT_HXX
#define UDP_TORRENT_CLIENT_HXX

#include "udp_socket.hxx"
#include "utility.hxx"

#include <QObject>
#include <QTimer>
#include <random>

class Udp_torrent_client : public QObject, public std::enable_shared_from_this<Udp_torrent_client> {
	Q_OBJECT
public:
	enum class Action_Code { 
		Connect, 
		Announce,
		Scrape,
		Error
	}; 

	enum class Download_Event {
		None,
		Started,
		Stopped,
		Completed
	}; 
	
	explicit Udp_torrent_client(bencode::Metadata torrent_metadata) : metadata_(std::move(torrent_metadata)){}

	std::shared_ptr<Udp_torrent_client> bind_lifetime() noexcept;
	void send_connect_requests() noexcept;
signals:
	void stop() const;
private:
	static QByteArray craft_connect_request() noexcept;
	QByteArray craft_announce_request(quint64_be tracker_connection_id) const noexcept;
	static QByteArray craft_scrape_request(const bencode::Metadata & metadata,quint64_be tracker_connection_id) noexcept;

	static std::optional<quint64_be> verify_connect_response(const QByteArray & response,std::uint32_t txn_id_sent) noexcept;
	static std::vector<QUrl> verify_announce_response(const QByteArray & response,Udp_socket * socket) noexcept;
	static void verify_scrape_response(const QByteArray & response,Udp_socket * socket) noexcept;

	void on_socket_ready_read(Udp_socket * socket) noexcept;
	///
	inline static std::mt19937 random_generator {std::random_device{}()};
	inline static std::uniform_int_distribution<std::uint32_t> random_id_range;
	inline static auto peer_id = QByteArray("-TA0001-01234501234567").toHex();
	constexpr static auto hex_base = 16;

	bencode::Metadata metadata_;
	quint64_be downloaded_ {};
	quint64_be left_ {};
	quint64_be uploaded_ {};
	Download_Event event_ {};
};

inline std::shared_ptr<Udp_torrent_client> Udp_torrent_client::bind_lifetime() noexcept {

	connect(this,&Udp_torrent_client::stop,this,[self = shared_from_this()]{
		assert(self.unique());
	},Qt::SingleShotConnection);

	return shared_from_this();
}

#endif // UDP_TORRENT_CLIENT_HXX