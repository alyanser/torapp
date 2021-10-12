#pragma once

#include "utility.hxx"

#include <bencode_parser.hxx>
#include <QCryptographicHash>
#include <QBitArray>
#include <QObject>
#include <QTimer>
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

         void begin_download(const QList<QUrl> & peer_urls) noexcept;
signals:
         void piece_downloaded(std::int32_t piece_idx) const;
         void existing_pieces_verified() const;
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
         void on_bitfield_received(Tcp_socket * socket,const QByteArray & response,std::int32_t payload_size) noexcept;
         void on_piece_received(Tcp_socket * socket,const QByteArray & response) noexcept;
         void on_allowed_fast_received(Tcp_socket * socket,std::int32_t allowed_piece_idx) noexcept;
         void on_piece_downloaded(Piece & piece,std::int32_t downloaded_piece_idx) noexcept;
         void on_piece_request_received(Tcp_socket * socket,const QByteArray & response) noexcept;

         static std::optional<std::pair<QByteArray,QByteArray>> verify_handshake_response(Tcp_socket * socket);
         static std::tuple<std::int32_t,std::int32_t,std::int32_t> extract_piece_metadata(const QByteArray & response);
         bool verify_piece_hash(const QByteArray & received_piece,std::int32_t piece_idx) const noexcept;

         void extract_peer_response(const QByteArray & peer_response) const noexcept;
         void communicate_with_peer(Tcp_socket * socket);
         Piece_metadata get_piece_info(std::int32_t piece_idx,std::int32_t offset = 0) const noexcept;
         void send_block_requests(Tcp_socket * socket,std::int32_t piece_idx) noexcept;
         std::int32_t get_target_piece_idx() const noexcept;
         bool write_to_disk(const QByteArray & received_piece,std::int32_t received_piece_idx) noexcept;
         std::optional<QByteArray> read_from_disk(std::int32_t requested_piece_idx) noexcept;
         std::optional<std::pair<qsizetype,qsizetype>> get_beginning_file_info(std::int32_t piece_idx) const noexcept;
         void verify_existing_pieces() noexcept;
         static bool is_valid_response(Tcp_socket * socket,const QByteArray & response,Message_Id received_msg_id) noexcept;
         std::int32_t get_piece_size(std::int32_t piece_idx) const noexcept;
         ///
         constexpr static std::string_view keep_alive_msg{"00000000"};
         constexpr static std::string_view choke_msg{"0000000100"};
         constexpr static std::string_view unchoke_msg{"0000000101"};
         constexpr static std::string_view interested_msg{"0000000102"};
         constexpr static std::string_view uninterested_msg{"0000000103"};
         constexpr static std::string_view have_all_msg{"000000010E"};
         constexpr static std::string_view have_none_msg{"000000010F"};
         constexpr static auto max_block_size = 1 << 14;
         inline static const auto reserved_bytes = QByteArray("\x00\x00\x00\x00\x00\x00\x00\x04",8).toHex();

         QSet<QUrl> active_peers_;
         QSet<std::int32_t> rem_pieces_;
         
         bencode::Metadata & torrent_metadata_;
         QList<std::int64_t> torrent_file_sizes_;
         QList<QFile *> file_handles_;
         QByteArray id_;
         QByteArray info_sha1_hash_;
         QByteArray handshake_msg_;
         QTimer acquire_piece_timer_;
         std::int64_t torrent_size_ = 0;
         std::int64_t piece_size_ = 0;
         std::int32_t total_piece_cnt_ = 0;
         std::int32_t spare_bit_cnt_ = 0;
         std::int32_t average_block_cnt_ = 0;
         QBitArray bitfield_;
         QList<Piece> pieces_;
         std::int32_t active_connection_cnt_ = 0;
         std::int32_t downloaded_piece_cnt_ = 0;
         Download_tracker * const tracker_ = nullptr;
};

inline Peer_wire_client::Peer_wire_client(bencode::Metadata & torrent_metadata,util::Download_resources resources,QByteArray id,QByteArray info_sha1_hash)
         : torrent_metadata_(torrent_metadata)
         , torrent_file_sizes_(static_cast<qsizetype>(torrent_metadata_.file_info.size()))
         , file_handles_(std::move(resources.file_handles))
         , id_(std::move(id))
         , info_sha1_hash_(std::move(info_sha1_hash))
         , handshake_msg_(craft_handshake_message())
         , torrent_size_(torrent_metadata_.single_file ? torrent_metadata_.single_file_size : torrent_metadata_.multiple_files_size)
         , piece_size_(torrent_metadata_.piece_length)
         , total_piece_cnt_(static_cast<std::int32_t>(std::ceil(static_cast<double>(torrent_size_) / static_cast<double>(piece_size_))))
         , spare_bit_cnt_(total_piece_cnt_ % 8 ? 8 - total_piece_cnt_ % 8 : 0)
         , average_block_cnt_(static_cast<std::int32_t>(std::ceil(static_cast<double>(piece_size_) / max_block_size)))
         , bitfield_(static_cast<qsizetype>(total_piece_cnt_ + spare_bit_cnt_))
         , pieces_(total_piece_cnt_)
         , tracker_(resources.tracker)
{
         assert(piece_size_);
         assert(tracker_);
         assert(!info_sha1_hash_.isEmpty());
         assert(!id_.isEmpty());
         assert(file_handles_.size() == static_cast<qsizetype>(torrent_metadata_.file_info.size()));
         assert(file_handles_.size() == torrent_file_sizes_.size());

         for(qsizetype idx = 0;idx < torrent_file_sizes_.size();++idx){
                  torrent_file_sizes_[idx] = torrent_metadata_.file_info[static_cast<std::size_t>(idx)].second;
                  assert(torrent_file_sizes_[idx]);
         }
}

[[nodiscard]]
inline std::int32_t Peer_wire_client::get_target_piece_idx() const noexcept {
         // todo: actually randomize the order
         assert(!rem_pieces_.empty());
         return *rem_pieces_.constBegin();
}

[[nodiscard]]
inline std::int32_t Peer_wire_client::get_piece_size(const std::int32_t piece_idx) const noexcept {
         assert(piece_idx >= 0 && piece_idx < total_piece_cnt_);
         const auto result = piece_idx == total_piece_cnt_ - 1 && torrent_size_ % piece_size_ ? torrent_size_ % piece_size_ : piece_size_;
         assert(result > 0 && result <= std::numeric_limits<std::int32_t>::max());
         return static_cast<std::int32_t>(result);
}