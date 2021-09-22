#ifndef UDP_TORRENT_CLIENT_HXX
#define UDP_TORRENT_CLIENT_HXX

#include "utility.hxx"

#include <QObject>

class quint32_t;

class Udp_torrent_client : public QObject, public std::enable_shared_from_this<Udp_torrent_client> {
	Q_OBJECT
public:
	enum class Action_Code { Connect, Announce };
	enum Packet_Segment_Bytes { Action_Code_Bytes = 4, Transaction_Id_Bytes = 4, Protocol_Constant_Bytes = 8 };

	explicit Udp_torrent_client(bencode::Metadata torrent_metadata) : metadata_(std::move(torrent_metadata)){}

	static QByteArray craft_connect_request() noexcept;
	static std::optional<std::uint64_t> verify_connect_response(const QByteArray & request,const QByteArray & response) noexcept;
	auto run() noexcept;
	void send_connect_requests() noexcept;
signals:
	void stop() const;
private:
	bencode::Metadata metadata_;
};

inline auto Udp_torrent_client::run() noexcept {

	connect(this,&Udp_torrent_client::stop,this,[self = shared_from_this()]{
		assert(self.unique());
	},Qt::SingleShotConnection);

	return shared_from_this();
}

#endif // UDP_TORRENT_CLIENT_HXX