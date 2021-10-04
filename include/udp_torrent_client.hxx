#pragma once

#include "udp_socket.hxx"
#include "peer_wire_client.hxx"
#include "utility.hxx"

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
                  std::uint32_t seed_count = 0;
                  std::uint32_t completed_count = 0;
                  std::uint32_t leecher_count = 0;
         };

         struct Announce_response {
                  std::vector<QUrl> peer_urls;
                  std::uint32_t interval_time = 0;
                  std::uint32_t leecher_count = 0;
                  std::uint32_t seed_count = 0;
         };

         using connect_optional = std::optional<quint64_be>;
         using scrape_optional = std::optional<Swarm_metadata>;
         using announce_optional = std::optional<Announce_response>;
         using error_optional = std::optional<QByteArray>;
         
         explicit Udp_torrent_client(bencode::Metadata torrent_metadata,QObject * parent);

         void send_connect_request() noexcept;
signals:
         void announce_response_received(const Announce_response & announce_response) const;
         void swarm_metadata_received(const Swarm_metadata & swarm_metadata) const;
         void error_received(const QByteArray & array) const;
private:
         static QByteArray craft_connect_request() noexcept;
         static QByteArray craft_scrape_request(const bencode::Metadata & metadata,quint64_be tracker_connection_id) noexcept;
         QByteArray craft_announce_request(quint64_be tracker_connection_id) const noexcept;

         static connect_optional extract_connect_response(const QByteArray & response,std::uint32_t sent_txn_id);
         static announce_optional extract_announce_response(const QByteArray & response,std::uint32_t sent_txn_id);
         static scrape_optional extract_scrape_response(const QByteArray & response,std::uint32_t sent_txn_id);
         static error_optional extract_tracker_error(const QByteArray & response,std::uint32_t sent_txn_id);

         static bool verify_txn_id(const QByteArray & response,std::uint32_t sent_txn_id);
         static QByteArray calculate_info_sha1_hash(const bencode::Metadata & metadata) noexcept;

         void on_socket_ready_read(Udp_socket * socket);
         void configure_default_connections() noexcept;
         ///
         inline static std::mt19937 random_generator {std::random_device{}()};
         inline static std::uniform_int_distribution<std::uint32_t> random_id_range;
         inline const static auto id = QByteArray("-TA0001-ABC134ALIlli").toHex();

         std::uint64_t downloaded_ {};
         std::uint64_t uploaded_ {};
         Download_Event event_ {};

         bencode::Metadata metadata_;
         QByteArray info_sha1_hash_;
         Peer_wire_client peer_client_;
         std::uint64_t total_ = 0;
         std::uint64_t left_ {};
};

inline Udp_torrent_client::Udp_torrent_client(bencode::Metadata torrent_metadata,QObject * const parent)
         : QObject(parent)
         , metadata_(std::move(torrent_metadata))
         , info_sha1_hash_(calculate_info_sha1_hash(metadata_))
         , peer_client_(metadata_,id,info_sha1_hash_)
, total_(metadata_.single_file ? metadata_.single_file_size : metadata_.multiple_files_size)
         , left_(total_)
{
         configure_default_connections();
}

inline QByteArray Udp_torrent_client::calculate_info_sha1_hash(const bencode::Metadata & metadata) noexcept {
         const auto raw_info_size = static_cast<std::ptrdiff_t>(metadata.raw_info_dict.size());
         return QCryptographicHash::hash(QByteArray(metadata.raw_info_dict.data(),raw_info_size),QCryptographicHash::Sha1).toHex();
}