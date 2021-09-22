#ifndef UDP_TORRENT_CLIENT_HXX
#define UDP_TORRENT_CLIENT_HXX

#include "utility.hxx"

#include <QUdpSocket>
#include <QObject>
#include <random>

class quint32_t;

class Udp_torrent_client : public QObject, public std::enable_shared_from_this<Udp_torrent_client> {
	Q_OBJECT
public:
	enum class Action_Code { 
		Connect, 
		Announce 
	};

	enum Connect_Segment_Bytes { 
		Action_Code_Bytes = 4, 
		Transaction_Id_Bytes = 4,
		Protocol_Constant_Bytes = 8,
	};

	enum class Download_event {
		None,
		Started,
		Stopped,
		Completed
	};

	explicit Udp_torrent_client(bencode::Metadata torrent_metadata) : metadata_(std::move(torrent_metadata)){}

	template<typename Socket_T,typename Packet_T,typename Size_T>
	static void send_packet(Socket_T && socket,Packet_T && packet,Size_T packet_size) noexcept;

	void send_connect_requests() noexcept;
	auto run() noexcept;
signals:
	void stop() const;
private:
	static QByteArray craft_connect_request() noexcept;
	QByteArray craft_announce_request(std::uint64_t server_connection_id) const noexcept;
	static std::optional<quint64_be> verify_connect_response(const QByteArray & request,const QByteArray & response) noexcept;
	std::vector<QUrl> verify_announce_response(const QByteArray & response) const noexcept;
	///
	inline static std::mt19937 random_generator = std::mt19937(std::random_device{}());
	inline static std::uniform_int_distribution<std::uint32_t> random_id_range;
	inline static QByteArray peer_id = QByteArray("-TA0001-01234501234567").toHex();

	QSet<std::uint32_t> announced_random_ids_;
	bencode::Metadata metadata_;
	quint64_be downloaded_ = quint64_be(0);
	quint64_be left_ = quint64_be(0);
	quint64_be uploaded_ = quint64_be(0);
	Download_event event_ = Download_event::None;
	std::uint8_t timeout_factor_ = 0;
};

inline auto Udp_torrent_client::run() noexcept {

	connect(this,&Udp_torrent_client::stop,this,[self = shared_from_this()]{
		assert(self.unique());
	},Qt::SingleShotConnection);

	return shared_from_this();
}

template<typename Socket_T,typename Packet_T,typename Size_T>
void Udp_torrent_client::send_packet(Socket_T && socket,Packet_T && packet_type,const Size_T packet_size) noexcept {
	socket.write(std::forward<Packet_T>(packet_type),packet_size);
}

#endif // UDP_TORRENT_CLIENT_HXX