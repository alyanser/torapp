#pragma once

#include "torrent_properties_displayer.hxx"
#include "util.hxx"

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

         enum State {
                  Verification,
                  Leecher,
                  Seed
         };

         Q_ENUM(State);

         Peer_wire_client(bencode::Metadata & torrent_metadata,util::Download_resources resources,QByteArray id,QByteArray info_sha1_hash);

         std::int64_t downloaded_byte_count() const noexcept;
         std::int64_t uploaded_byte_count() const noexcept;
         std::int64_t remaining_byte_count() const noexcept;

         void connect_to_peers(const QList<QUrl> & peer_urls) noexcept;
signals:
         void piece_verified(std::int32_t piece_idx) const;
         void existing_pieces_verified() const;
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

         struct File_info {
                  QFile * file_handle;
                  std::int64_t dled_byte_cnt;
         };

         template<Message_Id message_id>
         QByteArray craft_message(std::int32_t piece_idx,std::int32_t piece_offset,std::int32_t byte_cnt = 0) const noexcept;
         
         static QByteArray craft_have_message(std::int32_t piece_idx) noexcept;
         static QByteArray craft_piece_message(const QByteArray & piece_data,std::int32_t piece_idx,std::int32_t piece_offset) noexcept;
         static QByteArray craft_bitfield_message(const QBitArray & bitfield) noexcept;
         static QByteArray craft_allowed_fast_message(std::int32_t piece_idx) noexcept;
         QByteArray craft_handshake_message() const noexcept;

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
         std::int32_t piece_size(std::int32_t piece_idx) const noexcept;

         static QSet<std::int32_t> generate_allowed_fast_set(std::uint32_t peer_ip,std::int32_t total_piece_cnt) noexcept;
         void clear_piece(std::int32_t piece_idx) noexcept;
         void configure_default_connections() noexcept;
         void update_target_piece_indexes() noexcept;
         ///
         constexpr static std::string_view keep_alive_msg{"00000000"};
         constexpr static std::string_view choke_msg{"0000000100"};
         constexpr static std::string_view unchoke_msg{"0000000101"};
         constexpr static std::string_view interested_msg{"0000000102"};
         constexpr static std::string_view uninterested_msg{"0000000103"};
         constexpr static std::string_view have_all_msg{"000000010e"};
         constexpr static std::string_view have_none_msg{"000000010f"};
         constexpr static std::string_view reserved_bytes{"0000000000000004"};
         constexpr static std::int16_t max_block_size = 1 << 14;
         QList<std::pair<QFile *,std::int64_t>> file_handles_; // {file_handle,count of bytes downloaded}
         QList<QUrl> active_peers_;
         QList<std::int32_t> target_piece_idxes_;
         Torrent_properties_displayer properties_displayer_;
         QByteArray id_;
         QByteArray info_sha1_hash_;
         QByteArray handshake_msg_;
         QString dl_path_;
         QBitArray bitfield_;
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
         std::int32_t dled_piece_cnt_ = 0;
         State state_ = State::Verification;

         QList<std::int32_t> peer_additive_bitfield_;
         QList<Piece> pieces_;
};

[[nodiscard]]
inline std::int64_t Peer_wire_client::downloaded_byte_count() const noexcept {
         return dled_byte_cnt_;
}

[[nodiscard]]
inline std::int64_t Peer_wire_client::uploaded_byte_count() const noexcept {
         return uled_byte_cnt_;
}

[[nodiscard]]
inline std::int64_t Peer_wire_client::remaining_byte_count() const noexcept {
         return total_byte_cnt_ - dled_byte_cnt_;
}

[[nodiscard]]
inline bool Peer_wire_client::is_valid_piece_index(const std::int32_t piece_idx) const noexcept {
         return piece_idx >= 0 && piece_idx < total_piece_cnt_;
}

[[nodiscard]]
inline qsizetype Peer_wire_client::file_size(const qsizetype file_idx) const noexcept {
         return static_cast<qsizetype>(torrent_metadata_.file_info[static_cast<std::size_t>(file_idx)].second);
}

template<Peer_wire_client::Message_Id msg_id>
[[nodiscard]]
QByteArray Peer_wire_client::craft_message(const std::int32_t piece_idx,const std::int32_t piece_offset, [[maybe_unused]] const std::int32_t byte_cnt) const noexcept {
         static_assert(msg_id == Message_Id::Reject_Request || msg_id == Message_Id::Request || msg_id == Message_Id::Cancel,"Only valid for specified message id types");

         using util::conversion::convert_to_hex;

         auto msg = []{
                  constexpr auto packet_size = 13;
                  return convert_to_hex(packet_size);
         }();

         constexpr auto fin_msg_size = 34;
         msg.reserve(fin_msg_size);

         msg += convert_to_hex(static_cast<std::int8_t>(msg_id));
         msg += convert_to_hex(piece_idx);
         msg += convert_to_hex(piece_offset);

         if constexpr (msg_id == Message_Id::Reject_Request){
                  msg += convert_to_hex(byte_cnt);
         }else{
                  msg += convert_to_hex(piece_info(piece_idx,piece_offset).block_size);
         }

         assert(msg.size() == fin_msg_size);
         return msg;
}