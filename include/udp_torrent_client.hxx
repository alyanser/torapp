#ifndef UDP_TORRENT_CLIENT_HXX
#define UDP_TORRENT_CLIENT_HXX

#include "utility.hxx"

#include <QUdpSocket>
#include <QObject>
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

	enum class Download_event {
		None,
		Started,
		Stopped,
		Completed
	};

	class Udp_socket : public QUdpSocket {
	public:
		void set_txn_id(const std::uint32_t new_txn_id) noexcept {
			txn_id_ = new_txn_id;
		}
	
		[[nodiscard]]
		std::uint32_t txn_id() const noexcept {
			return txn_id_;
		}
	private:
		std::uint32_t txn_id_;
	};

	explicit Udp_torrent_client(bencode::Metadata torrent_metadata) : metadata_(std::move(torrent_metadata)){}


	std::shared_ptr<Udp_torrent_client> run() noexcept;
	void send_connect_requests() noexcept;
	void on_socket_ready_read(Udp_socket & socket,const QByteArray & connect_request) noexcept;
signals:
	void stop() const;
private:
	static void send_packet(Udp_socket & socket,const QByteArray & packet) noexcept;
	static QByteArray craft_connect_request() noexcept;
	static std::optional<quint64_be> verify_connect_response(const QByteArray & response,std::uint32_t txn_id_sent) noexcept;
	static std::vector<QUrl> verify_announce_response(const QByteArray & response,std::uint32_t txn_id_sent) noexcept;
	QByteArray craft_announce_request(std::uint64_t server_connection_id) const noexcept;
	///
	inline static auto random_generator = std::mt19937(std::random_device{}());
	inline static auto peer_id = QByteArray("-TA0001-01234501234567").toHex();
	inline static std::uniform_int_distribution<std::uint32_t> random_id_range;

	bencode::Metadata metadata_;
	quint64_be downloaded_ = quint64_be(0);
	quint64_be left_ = quint64_be(0);
	quint64_be uploaded_ = quint64_be(0);
	Download_event event_ = Download_event::None;
	std::uint8_t timeout_factor_ = 0;
};

inline std::shared_ptr<Udp_torrent_client> Udp_torrent_client::run() noexcept {

	connect(this,&Udp_torrent_client::stop,this,[self = shared_from_this()]{
		assert(self.unique());
	},Qt::SingleShotConnection);

	return shared_from_this();
}

inline void Udp_torrent_client::send_packet(Udp_socket & socket,const QByteArray & packet) noexcept {
	socket.write(packet.data(),packet.size());

	constexpr auto txn_id_offset = 4;
	constexpr auto txn_id_bytes = 4;

	socket.set_txn_id(packet.sliced(txn_id_offset,txn_id_bytes).toHex().toUInt());
}

#endif // UDP_TORRENT_CLIENT_HXX