#pragma once

#include "udp_socket.hxx"
#include "peer_wire_client.hxx"
#include "utility.hxx"

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
                  Invalid
         };

         enum class Download_Event {
                  None,
                  Started,
                  Stopped,
                  Completed
         };

         struct Swarm_metadata {
                  std::int32_t seed_cnt = 0;
                  std::int32_t completed_cnt = 0;
                  std::int32_t leecher_cnt = 0;
         };

         struct Announce_response {
                  QList<QUrl> peer_urls;
                  std::int32_t interval_time = 0;
                  std::int32_t leecher_cnt = 0;
                  std::int32_t seed_cnt = 0;
         };

         Udp_torrent_client(bencode::Metadata torrent_metadata,util::Download_resources resources,QObject * parent = nullptr);

         void send_connect_request() noexcept;
signals:
         void announce_response_received(const Announce_response & announce_response) const;
         void swarm_metadata_received(const Swarm_metadata & swarm_metadata) const;
         void error_received(const QByteArray & array) const;
private:
         static QByteArray craft_connect_request() noexcept;
         static QByteArray craft_scrape_request(const bencode::Metadata & metadata,std::int64_t tracker_connection_id) noexcept;
         QByteArray craft_announce_request(std::int64_t tracker_connection_id) const noexcept;

         static std::optional<std::int64_t> extract_connect_response(const QByteArray & response,std::int32_t sent_txn_id);
         static std::optional<Announce_response> extract_announce_response(const QByteArray & response,std::int32_t sent_txn_id);
         static std::optional<Swarm_metadata> extract_scrape_response(const QByteArray & response,std::int32_t sent_txn_id);
         static std::optional<QByteArray> extract_tracker_error(const QByteArray & response,std::int32_t sent_txn_id);

         static bool verify_txn_id(const QByteArray & response,std::int32_t sent_txn_id);
         static QByteArray calculate_info_sha1_hash(const bencode::Metadata & metadata) noexcept;

         void on_socket_ready_read(Udp_socket * socket);
         void configure_default_connections() noexcept;
         ///
         inline static std::mt19937 random_generator {std::random_device{}()};
         inline static std::uniform_int_distribution<std::int32_t> random_id_range;
         inline const static auto id = QByteArray("-TA0001-ABC134ZXClli").toHex();

         bencode::Metadata torrent_metadata_;
         QByteArray info_sha1_hash_;
         Peer_wire_client peer_client_;
         Download_tracker * tracker_ = nullptr;
         std::int64_t total_ = 0;
         std::int64_t left_ = 0;
         std::int64_t downloaded_ = 0;
         std::int64_t uploaded_ = 0;
         Download_Event event_{Download_Event::None};
};

inline Udp_torrent_client::Udp_torrent_client(bencode::Metadata torrent_metadata,util::Download_resources resources,QObject * const parent)
         : QObject(parent)
         , torrent_metadata_(std::move(torrent_metadata))
         , info_sha1_hash_(calculate_info_sha1_hash(torrent_metadata_))
         , peer_client_(torrent_metadata_,std::move(resources.file_handles),id,info_sha1_hash_)
         , tracker_(resources.tracker)
         , total_(torrent_metadata_.single_file ? torrent_metadata_.single_file_size : torrent_metadata_.multiple_files_size)
         , left_(total_)
{
         configure_default_connections();

         if(torrent_metadata_.announce_url_list.empty()){
                  assert(!torrent_metadata_.announce_url.empty());
                  torrent_metadata_.announce_url_list.push_back(torrent_metadata_.announce_url);
         }
}

[[nodiscard]]
inline QByteArray Udp_torrent_client::calculate_info_sha1_hash(const bencode::Metadata & torrent_metadata) noexcept {
         const auto raw_info_size = static_cast<qsizetype>(torrent_metadata.raw_info_dict.size());
         return QCryptographicHash::hash(QByteArray(torrent_metadata.raw_info_dict.data(),raw_info_size),QCryptographicHash::Sha1).toHex();
}