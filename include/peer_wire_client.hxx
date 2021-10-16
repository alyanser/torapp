#pragma once

#include "util.hxx"

#include <bencode_parser.hxx>
#include <QBitArray>
#include <QObject>
#include <QTimer>
#include <QDebug>
#include <QSet>
#include <random>

class Tcp_socket;

class Peer_wire_client : public QObject {
         Q_OBJECT
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
         Peer_wire_client(const Peer_wire_client & rhs) = delete;
         Peer_wire_client(Peer_wire_client && rhs) = delete;
         Peer_wire_client & operator = (const Peer_wire_client & rhs) = delete;
         Peer_wire_client & operator = (Peer_wire_client && rhs) = delete;
         ~Peer_wire_client() override;

         constexpr std::int64_t downloaded_byte_count() const noexcept;
         constexpr std::int64_t uploaded_byte_count() const noexcept;
         constexpr std::int64_t remaining_byte_count() const noexcept;

         void connect_to_peers(const QList<QUrl> & peer_urls) noexcept;
signals:
         void piece_verified(std::int32_t piece_idx) const;
         void existing_pieces_verified() const;
         void download_paused() const;
         void download_finished() const;
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
                  std::int32_t total_block_cnt = 0;
         };

         std::int32_t get_piece_size(std::int32_t piece_idx) const noexcept;

         static QByteArray craft_have_message(std::int32_t piece_idx) noexcept;
         static QByteArray craft_piece_message(const QByteArray & piece_data,std::int32_t piece_idx,std::int32_t offset) noexcept;
         static QByteArray craft_bitfield_message(const QBitArray & bitfield) noexcept;
         QByteArray craft_request_message(std::int32_t piece_idx,std::int32_t offset) const noexcept;
         QByteArray craft_cancel_message(std::int32_t piece_idx,std::int32_t offset) const noexcept;
         static QByteArray craft_allowed_fast_message(std::int32_t piece_idx) noexcept;
         QByteArray craft_handshake_message() const noexcept;
         static QByteArray craft_reject_message(std::int32_t piece_idx,std::int32_t piece_offset,std::int32_t byte_cnt) noexcept;

         void on_socket_ready_read(Tcp_socket * socket) noexcept;
         void on_unchoke_message_received(Tcp_socket * socket) noexcept;
         void on_have_message_received(Tcp_socket * socket,std::int32_t peer_have_piece_idx) noexcept;
         void on_bitfield_received(Tcp_socket * socket) noexcept;
         void on_piece_received(Tcp_socket * socket,const QByteArray & reply) noexcept;
         void on_allowed_fast_received(Tcp_socket * socket,std::int32_t allowed_piece_idx) noexcept;
         void on_piece_downloaded(QPointer<Tcp_socket> socket,Piece & dled_piece,std::int32_t dled_piece_idx) noexcept;
         void on_piece_request_received(Tcp_socket * socket,const QByteArray & request) noexcept;
         void on_suggest_piece_received(Tcp_socket * socket,std::int32_t suggested_piece_idx) noexcept;
         void send_block_requests(Tcp_socket * socket,std::int32_t piece_idx) noexcept;
         void on_socket_connected(Tcp_socket * socket) noexcept;

         static std::optional<std::pair<QByteArray,QByteArray>> verify_handshake_reply(Tcp_socket * socket,const QByteArray & reply);
         void verify_existing_pieces() noexcept;
         bool verify_piece_hash(const QByteArray & received_piece,std::int32_t piece_idx) const noexcept;

         static std::tuple<std::int32_t,std::int32_t,std::int32_t> extract_piece_metadata(const QByteArray & reply);
         void extract_peer_reply(const QByteArray & peer_reply) const noexcept;
         void communicate_with_peer(Tcp_socket * socket);
         Piece_metadata get_piece_info(std::int32_t piece_idx,std::int32_t offset = 0) const noexcept;
         qsizetype get_file_size(qsizetype file_idx) const noexcept;

         bool write_to_disk(const QByteArray & received_piece,std::int32_t received_piece_idx) noexcept;
         std::optional<QByteArray> read_from_disk(std::int32_t requested_piece_idx) noexcept;
         std::optional<std::pair<qsizetype,qsizetype>> get_beginning_file_info(std::int32_t piece_idx) const noexcept;
         static bool is_valid_reply(Tcp_socket * socket,const QByteArray & reply,Message_Id received_msg_id) noexcept;
         void write_settings() const noexcept;
         void read_settings() noexcept;
         ///
         constexpr static std::string_view keep_alive_msg{"00000000"};
         constexpr static std::string_view choke_msg{"0000000100"};
         constexpr static std::string_view unchoke_msg{"0000000101"};
         constexpr static std::string_view interested_msg{"0000000102"};
         constexpr static std::string_view uninterested_msg{"0000000103"};
         constexpr static std::string_view have_all_msg{"000000010E"};
         constexpr static std::string_view have_none_msg{"000000010F"};
         constexpr static auto max_block_size = 1 << 14;
         constexpr static std::string_view reserved_bytes{"0000000000000004"}; // fast extension bitw
         QByteArray id_;
         QByteArray info_sha1_hash_;
         QByteArray handshake_msg_;
         QString dl_path_;
         QBitArray bitfield_;
         QList<QFile *> file_handles_;
         QSet<QUrl> active_peers_;
         QSet<std::int32_t> allowed_fast_pieces;
         QTimer acquire_piece_timer_;
         bencode::Metadata & torrent_metadata_;
         Download_tracker * const tracker_ = nullptr;
         std::int64_t dled_byte_cnt_ = 0;
         std::int64_t uled_byte_cnt_ = 0;
         std::int64_t total_byte_cnt_ = 0;
         std::int64_t piece_size_ = 0;
         std::int32_t total_piece_cnt_ = 0;
         std::int32_t spare_bit_cnt_ = 0;
         std::int32_t average_block_cnt_ = 0;
         std::int32_t active_connection_cnt_ = 0;
         std::int32_t dled_piece_cnt_ = 0;
         QList<Piece> pieces_;
};

inline Peer_wire_client::~Peer_wire_client() {
         write_settings();
}

[[nodiscard]]
constexpr std::int64_t Peer_wire_client::downloaded_byte_count() const noexcept {
         return dled_byte_cnt_;
}

[[nodiscard]]
constexpr std::int64_t Peer_wire_client::uploaded_byte_count() const noexcept {
         return uled_byte_cnt_;
}

[[nodiscard]]
constexpr std::int64_t Peer_wire_client::remaining_byte_count() const noexcept {
         return total_byte_cnt_ - dled_byte_cnt_;
}

[[nodiscard]]
inline std::int32_t Peer_wire_client::get_piece_size(const std::int32_t piece_idx) const noexcept {
         assert(piece_idx >= 0 && piece_idx < total_piece_cnt_);
         const auto result = piece_idx == total_piece_cnt_ - 1 && total_byte_cnt_ % piece_size_ ? total_byte_cnt_ % piece_size_ : piece_size_;
         assert(result > 0 && result <= std::numeric_limits<std::int32_t>::max());
         return static_cast<std::int32_t>(result);
}

[[nodiscard]]
inline qsizetype Peer_wire_client::get_file_size(const qsizetype file_idx) const noexcept {
         assert(static_cast<qsizetype>(torrent_metadata_.file_info[static_cast<std::size_t>(file_idx)].second) > 0);
         return static_cast<qsizetype>(torrent_metadata_.file_info[static_cast<std::size_t>(file_idx)].second);
}