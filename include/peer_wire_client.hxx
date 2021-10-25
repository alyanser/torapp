#pragma once

#include "util.hxx"
#include "torrent_properties_displayer.hxx"

#include <bencode_parser.hxx>
#include <QBitArray>
#include <QObject>
#include <QTimer>
#include <QSet>
#include <random>

class Tcp_socket;

class Peer_wire_client : public QObject {
         Q_OBJECT
         
         struct Request_metadata;
public:
         enum class Message_Id {
                  Choke,
                  Unchoke,
                  Interested,
                  Uninterested,
                  Have,
                  Bitfield,
                  Request,
                  Piece,
                  Cancel,
                  Suggest_Piece = 13,
                  Have_All,
                  Have_None,
                  Reject_Request,
                  Allowed_Fast
         };

         Q_ENUM(Message_Id);

         Peer_wire_client(bencode::Metadata & torrent_metadata,util::Download_resources resources,QByteArray id,QByteArray info_sha1_hash);

         constexpr std::int64_t downloaded_byte_count() const noexcept;
         constexpr std::int64_t uploaded_byte_count() const noexcept;
         constexpr std::int64_t remaining_byte_count() const noexcept;

         void connect_to_peers(const QList<QUrl> & peer_urls) noexcept;
signals:
         void piece_verified(std::int32_t piece_idx) const;
         void existing_pieces_verified() const;
         void download_paused() const;
         void download_finished() const;
         void send_requests() const;
         void request_rejected(Request_metadata request_metadata) const;
private:
         struct Piece {
                  QList<std::int8_t> requested_blocks;
                  QBitArray received_blocks;
                  QByteArray data;
                  std::int32_t received_block_cnt = 0;
         };

         struct Piece_metadata {
                  std::int32_t piece_size = 0;
                  std::int32_t block_size = 0;
                  std::int32_t block_cnt = 0;
         };

         struct Request_metadata {
                  std::int32_t piece_index = 0;
                  std::int32_t piece_offset = 0;
                  std::int32_t byte_cnt = 0;
         };
         
         static QByteArray craft_have_message(std::int32_t piece_idx) noexcept;
         static QByteArray craft_piece_message(const QByteArray & piece_data,std::int32_t piece_idx,std::int32_t piece_offset) noexcept;
         static QByteArray craft_bitfield_message(const QBitArray & bitfield) noexcept;
         QByteArray craft_request_message(std::int32_t piece_idx,std::int32_t piece_offset) const noexcept;
         QByteArray craft_cancel_message(std::int32_t piece_idx,std::int32_t piece_offset) const noexcept;
         static QByteArray craft_allowed_fast_message(std::int32_t piece_idx) noexcept;
         QByteArray craft_handshake_message() const noexcept;
         static QByteArray craft_reject_message(std::int32_t piece_idx,std::int32_t piece_offset,std::int32_t byte_cnt) noexcept;

         void on_socket_ready_read(Tcp_socket * socket) noexcept;
         void on_have_message_received(Tcp_socket * socket,std::int32_t peer_have_piece_idx) noexcept;
         void on_bitfield_received(Tcp_socket * socket) noexcept;
         void on_block_received(Tcp_socket * socket,const QByteArray & reply) noexcept;
         void on_allowed_fast_received(Tcp_socket * socket,std::int32_t allowed_piece_idx) noexcept;
         void on_piece_downloaded(Piece & dled_piece,std::int32_t dled_piece_idx) noexcept;
         void on_block_request_received(Tcp_socket * socket,const QByteArray & request) noexcept;
         void on_suggest_piece_received(Tcp_socket * socket,std::int32_t suggested_piece_idx) noexcept;
         void send_block_requests(Tcp_socket * socket,std::int32_t piece_idx) noexcept;
         void on_socket_connected(Tcp_socket * socket) noexcept;
         void on_handshake_reply_received(Tcp_socket * socket,const QByteArray & reply);
         void on_piece_verified(std::int32_t verified_piece_idx) noexcept;

         static std::optional<std::pair<QByteArray,QByteArray>> verify_handshake_reply(Tcp_socket * socket,const QByteArray & reply);
         void verify_existing_pieces() noexcept;
         bool verify_piece_hash(const QByteArray & received_piece,std::int32_t piece_idx) const noexcept;

         static Request_metadata extract_piece_metadata(const QByteArray & reply);
         void communicate_with_peer(Tcp_socket * socket);
         Piece_metadata piece_info(std::int32_t piece_idx,std::int32_t piece_offset = 0) const noexcept;
         qsizetype file_size(qsizetype file_idx) const noexcept;

         bool write_to_disk(const QByteArray & received_piece,std::int32_t received_piece_idx) noexcept;
         std::optional<QByteArray> read_from_disk(std::int32_t requested_piece_idx) noexcept;

         void write_settings() const noexcept;
         void read_settings() noexcept;

         static bool is_valid_reply(Tcp_socket * socket,const QByteArray & reply,Message_Id received_msg_id) noexcept;
         bool is_valid_piece_index(std::int32_t piece_idx) const noexcept;

         std::optional<std::pair<qsizetype,qsizetype>> beginning_file_handle_info(std::int32_t piece_idx) const noexcept;
         std::int32_t target_piece_index() const noexcept;
         std::int32_t piece_size(std::int32_t piece_idx) const noexcept;

         static QSet<std::int32_t> generate_allowed_fast_set(std::uint32_t peer_ip,std::int32_t total_piece_cnt) noexcept;
         void clear_piece(std::int32_t piece_idx) noexcept;
         void configure_default_connections() noexcept;
         ///
         constexpr static std::string_view keep_alive_msg{"00000000"};
         constexpr static std::string_view choke_msg{"0000000100"};
         constexpr static std::string_view unchoke_msg{"0000000101"};
         constexpr static std::string_view interested_msg{"0000000102"};
         constexpr static std::string_view uninterested_msg{"0000000103"};
         constexpr static std::string_view have_all_msg{"000000010e"};
         constexpr static std::string_view have_none_msg{"000000010f"};
         constexpr static auto max_block_size = 1 << 14;
         constexpr static std::string_view reserved_bytes{"0000000000000004"}; // fast extension bit
         Torrent_properties_displayer properties_displayer_;
         QByteArray id_;
         QByteArray info_sha1_hash_;
         QByteArray handshake_msg_;
         QString dl_path_;
         QBitArray bitfield_;
         QList<QFile *> file_handles_;
         QSet<QUrl> active_peers_;
         bencode::Metadata & torrent_metadata_;
         QTimer settings_timer_;
         QTimer refresh_timer_;
         Download_tracker * const tracker_ = nullptr;
         std::int64_t dled_byte_cnt_ = 0;
         std::int64_t uled_byte_cnt_ = 0;
         std::int64_t session_dled_byte_cnt_ = 0;
         std::int64_t session_uled_byte_cnt_ = 0;
         std::int64_t total_byte_cnt_ = 0;
         std::int64_t torrent_piece_size_ = 0;
         std::int32_t total_piece_cnt_ = 0;
         std::int32_t spare_bit_cnt_ = 0;
         std::int32_t average_block_cnt_ = 0;
         std::int32_t active_connection_cnt_ = 0;
         std::int32_t dled_piece_cnt_ = 0;
         std::optional<std::int32_t> cur_target_piece_idx_;
         QList<std::int32_t> peer_additive_bitfield_;
         QList<Piece> pieces_;
};

[[nodiscard]]
constexpr std::int64_t Peer_wire_client::downloaded_byte_count() const noexcept {
         assert(dled_byte_cnt_ >= 0 && dled_byte_cnt_ <= total_byte_cnt_);
         return dled_byte_cnt_;
}

[[nodiscard]]
constexpr std::int64_t Peer_wire_client::uploaded_byte_count() const noexcept {
         return uled_byte_cnt_;
}

[[nodiscard]]
constexpr std::int64_t Peer_wire_client::remaining_byte_count() const noexcept {
         assert(dled_byte_cnt_ >= 0 &&dled_byte_cnt_ <= total_byte_cnt_);
         assert(dled_piece_cnt_ >= 0 && dled_piece_cnt_ <= total_piece_cnt_);
         return total_byte_cnt_ - dled_byte_cnt_;
}

[[nodiscard]]
inline std::int32_t Peer_wire_client::piece_size(const std::int32_t piece_idx) const noexcept {
         assert(is_valid_piece_index(piece_idx));
         const auto piece_size = piece_idx == total_piece_cnt_ - 1 && total_byte_cnt_ % torrent_piece_size_ ? total_byte_cnt_ % torrent_piece_size_ : torrent_piece_size_;
         assert(piece_size > 0 && piece_size <= torrent_piece_size_);
         return static_cast<std::int32_t>(piece_size);
}

[[nodiscard]]
inline qsizetype Peer_wire_client::file_size(const qsizetype file_idx) const noexcept {
         assert(static_cast<qsizetype>(torrent_metadata_.file_info[static_cast<std::size_t>(file_idx)].second) > 0);
         return static_cast<qsizetype>(torrent_metadata_.file_info[static_cast<std::size_t>(file_idx)].second);
}

[[nodiscard]]
inline bool Peer_wire_client::is_valid_piece_index(const std::int32_t piece_idx) const noexcept {
         return piece_idx >= 0 && piece_idx < total_piece_cnt_;
}