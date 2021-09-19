#ifndef TORRENT_CLIENT_HXX
#define TORRENT_CLIENT_HXX

#include "utility.hxx"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QObject>
#include <random>
#include <memory>

namespace bencode {
	struct Metadata;
}

class Torrent_client : public QObject, public std::enable_shared_from_this<Torrent_client> {
	Q_OBJECT
public:
	enum State { Seeding, Choking, Interested };

	explicit Torrent_client(const bencode::Metadata & metadata);

	std::shared_ptr<Torrent_client> bind_lifetime();
signals:
	void send_request(const QNetworkRequest & request) const;
public slots:
	void on_response_arrived(QNetworkReply & reply);
private:
	void send_initial_request(const bencode::Metadata & torrent_metadata) noexcept;
	///
	inline const static QString peer_id = QString("-TA0001-123456789101-"); //todo get random suffix
};

inline Torrent_client::Torrent_client(const bencode::Metadata & metadata){
	send_initial_request(metadata);
}

inline std::shared_ptr<Torrent_client> Torrent_client::bind_lifetime(){

	const auto self_lifetime_connection = connect(this,&Torrent_client::send_request,[self = shared_from_this()]{
	});

	assert(self_lifetime_connection);
	
	return shared_from_this();
}

inline void Torrent_client::on_response_arrived(QNetworkReply & reply){

	connect(&reply,&QNetworkReply::finished,[&reply]{
		qInfo() << reply.readAll();
	});
}

inline void Torrent_client::send_initial_request(const bencode::Metadata & torrent_metadata) noexcept {
	QUrlQuery query;
	query.addQueryItem("downloaded","0");
	query.addQueryItem("uploaded","0");
	query.addQueryItem("left",torrent_metadata.announce_url.data());
	query.addQueryItem("peer-id",peer_id);
	query.addQueryItem("port","6881");
	query.addQueryItem("compact","1");
	query.addQueryItem("event","started");
	query.addQueryItem("info_hash",torrent_metadata.pieces.data());

	// QUrl url(torrent_metadata.announce_url.data());
	QUrl url("http://tracker.files.fm:6969/announce");
	url.setQuery(query);
	emit send_request(QNetworkRequest(url));
}

#endif // TORRENT_CLIENT_HXX