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

         enum class Download_Event {
                  None,
                  Started,
                  Stopped,
                  Completed
         };

         Q_ENUM(Download_Event);

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
         static QByteArray calculate_info_sha1_hash(const bencode::Metadata & torrent_metadata) noexcept;
         static bool verify_txn_id(const QByteArray & response,std::int32_t sent_txn_id);
         static QByteArray craft_connect_request() noexcept;
         static QByteArray craft_scrape_request(const bencode::Metadata & metadata,std::int64_t tracker_connection_id) noexcept;
         QByteArray craft_announce_request(std::int64_t tracker_connection_id) const noexcept;
         static std::optional<QByteArray> extract_tracker_error(const QByteArray & response,std::int32_t sent_txn_id);
         static std::optional<std::int64_t> extract_connect_response(const QByteArray & response,std::int32_t sent_txn_id);
         static std::optional<Announce_response> extract_announce_response(const QByteArray & response,std::int32_t sent_txn_id);
         static std::optional<Swarm_metadata> extract_scrape_response(const QByteArray & response,std::int32_t sent_txn_id);
         void on_socket_ready_read(Udp_socket * socket);
         void configure_default_connections() noexcept;
         ///
         inline static std::mt19937 random_generator{std::random_device{}()};
         inline static std::uniform_int_distribution<std::int32_t> random_id_range{0,std::numeric_limits<std::int32_t>::max()};
         inline const static auto id = QByteArray("-TA0001-ABC134ZXClli").toHex();

         bencode::Metadata torrent_metadata_;
         QByteArray info_sha1_hash_;
         Peer_wire_client peer_client_;
         Download_tracker * const tracker_ = nullptr;
         Download_Event event_{Download_Event::None};
};

inline Udp_torrent_client::Udp_torrent_client(bencode::Metadata torrent_metadata,util::Download_resources resources,QObject * const parent)
         : QObject(parent)
         , torrent_metadata_(std::move(torrent_metadata))
         , info_sha1_hash_(calculate_info_sha1_hash(torrent_metadata_))
         , peer_client_(torrent_metadata_,{std::move(resources.file_path),std::move(resources.file_handles),resources.tracker},id,info_sha1_hash_)
         , tracker_(resources.tracker)
{
         configure_default_connections();

         if(torrent_metadata_.announce_url_list.empty()){
                  assert(!torrent_metadata_.announce_url.empty());
                  torrent_metadata_.announce_url_list.emplace_back(torrent_metadata_.announce_url);
         }
}

[[nodiscard]]
inline QByteArray Udp_torrent_client::calculate_info_sha1_hash(const bencode::Metadata & torrent_metadata) noexcept {
         const auto raw_info_size = static_cast<qsizetype>(torrent_metadata.raw_info_dict.size());
         return QCryptographicHash::hash(QByteArray(torrent_metadata.raw_info_dict.data(),raw_info_size),QCryptographicHash::Sha1).toHex();
}

[[nodiscard]]
inline std::optional<QByteArray> Udp_torrent_client::extract_tracker_error(const QByteArray & response,const std::int32_t sent_txn_id){

         if(!verify_txn_id(response,sent_txn_id)){
                  return {};
         }

         constexpr auto error_offset = 8;
         return response.sliced(error_offset);
}

[[nodiscard]]
inline std::optional<std::int64_t> Udp_torrent_client::extract_connect_response(const QByteArray & response,const std::int32_t sent_txn_id){

         if(!verify_txn_id(response,sent_txn_id)){
                  return {};
         }

         constexpr auto connection_id_offset = 8;
         return util::extract_integer<std::int64_t>(response,connection_id_offset);
}

[[nodiscard]]
inline bool Udp_torrent_client::verify_txn_id(const QByteArray & response,const std::int32_t sent_txn_id){
         constexpr auto txn_id_offset = 4;
         const auto received_txn_id = util::extract_integer<std::int32_t>(response,txn_id_offset);
         return sent_txn_id == received_txn_id;
}