#pragma once

#include "udp_socket.hxx"
#include "peer_wire_client.hxx"
#include "util.hxx"

#include <bencode_parser.hxx>
#include <QCryptographicHash>
#include <QObject>
#include <random>

class Udp_torrent_client : public QObject {
         Q_OBJECT
public:
         enum class Action_Code { 
                  Connect,
                  Announce,
                  Scrape,
                  Error,
         };

         Q_ENUM(Action_Code);

         enum class Event {
                  None,
                  Started,
                  Stopped,
                  Completed
         };

         Q_ENUM(Event);

         struct Swarm_metadata {
                  std::int32_t seed_cnt = 0;
                  std::int32_t completed_cnt = 0;
                  std::int32_t leecher_cnt = 0;
         };

         struct Announce_reply {
                  QList<QUrl> peer_urls;
                  std::int32_t interval_time = 0;
                  std::int32_t leecher_cnt = 0;
                  std::int32_t seed_cnt = 0;
         };

         Udp_torrent_client(bencode::Metadata torrent_metadata,util::Download_resources resources,QObject * parent = nullptr);

         void send_connect_request() noexcept;
signals:
         void announce_reply_received(const Announce_reply & announce_reply) const;
         void swarm_metadata_received(const Swarm_metadata & swarm_metadata) const;
         void error_received(const QByteArray & array) const;
private:
         static QByteArray craft_connect_request() noexcept;
         QByteArray craft_scrape_request(std::int64_t tracker_connection_id) const noexcept;
         QByteArray craft_announce_request(std::int64_t tracker_connection_id) const noexcept;

         static std::optional<QByteArray> extract_tracker_error(const QByteArray & reply,std::int32_t sent_txn_id);
         static std::optional<std::int64_t> extract_connect_reply(const QByteArray & reply,std::int32_t sent_txn_id);
         static std::optional<Announce_reply> extract_announce_reply(const QByteArray & reply,std::int32_t sent_txn_id);
         static std::optional<Swarm_metadata> extract_scrape_reply(const QByteArray & reply,std::int32_t sent_txn_id);

         static QByteArray calculate_info_sha1_hash(const bencode::Metadata & torrent_metadata) noexcept;
         static bool verify_txn_id(const QByteArray & reply,std::int32_t sent_txn_id);
         void communicate_with_tracker(Udp_socket * socket);
         void configure_default_connections() noexcept;
         ///
         inline static std::mt19937 random_generator{std::random_device{}()};
         inline static std::uniform_int_distribution<std::int32_t> random_id_range;
         inline const static auto id = QByteArray("-TA1234-ABC134ZXCLLZ").toHex();

         bencode::Metadata torrent_metadata_;
         QByteArray info_sha1_hash_;
         Peer_wire_client peer_client_;
         Download_tracker * const tracker_ = nullptr;
         qsizetype tracker_url_idx_ = 0;
         Event event_ = Event::None;
};

[[nodiscard]]
inline QByteArray Udp_torrent_client::calculate_info_sha1_hash(const bencode::Metadata & torrent_metadata) noexcept {
         const auto raw_info_size = static_cast<qsizetype>(torrent_metadata.raw_info_dict.size());
         return QCryptographicHash::hash(QByteArray(torrent_metadata.raw_info_dict.data(),raw_info_size),QCryptographicHash::Sha1).toHex();
}

[[nodiscard]]
inline bool Udp_torrent_client::verify_txn_id(const QByteArray & reply,const std::int32_t sent_txn_id){
         constexpr auto txn_id_offset = 4;
         const auto received_txn_id = util::extract_integer<std::int32_t>(reply,txn_id_offset);
         return sent_txn_id == received_txn_id;
}