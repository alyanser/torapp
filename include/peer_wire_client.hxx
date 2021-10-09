#pragma once

#include "utility.hxx"

#include <bencode_parser.hxx>
#include <QCryptographicHash>
#include <QBitArray>
#include <QObject>
#include <QTimer>
#include <QSet>
#include <QDebug>
#include <QFile>
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

         Peer_wire_client(bencode::Metadata torrent_metadata,std::vector<QFile *> file_handles,QByteArray peer_id,QByteArray info_sha1_hash);

         void do_handshake(const std::vector<QUrl> & peer_urls) noexcept;
signals:
         void piece_downloaded(std::uint32_t piece_idx) const;
private:
         struct Piece {
                  std::vector<std::uint8_t> requested_blocks;
                  std::vector<bool> received_blocks; // ? consider QBitArray
                  QByteArray data;
                  std::uint32_t received_block_cnt = 0;
         };

         struct Piece_metadata {
                  std::uint32_t piece_size = 0;
                  std::uint32_t block_size = 0;
                  std::uint32_t total_block_cnt = 0;
         };

         static QByteArray craft_have_message(std::uint32_t piece_idx) noexcept;
         static QByteArray craft_piece_message(const QByteArray & piece_data,std::uint32_t piece_idx,std::uint32_t offset) noexcept;
         static QByteArray craft_bitfield_message(const QBitArray & bitfield) noexcept;
         QByteArray craft_request_message(std::uint32_t piece_idx,std::uint32_t offset) const noexcept;
         QByteArray craft_cancel_message(std::uint32_t piece_idx,std::uint32_t offset) const noexcept;
         static QByteArray craft_allowed_fast_message(std::uint32_t piece_idx) noexcept;
         QByteArray craft_handshake_message() const noexcept;

         static std::optional<std::pair<QByteArray,QByteArray>> verify_handshake_response(Tcp_socket * socket);
         void on_socket_ready_read(Tcp_socket * socket) noexcept;
         bool verify_piece_hash(const QByteArray & received_piece,std::uint32_t piece_idx) const noexcept;

         void on_unchoke_message_received(Tcp_socket * socket) noexcept;
         void on_have_message_received(Tcp_socket * socket,std::uint32_t peer_have_piece_idx) noexcept;
         void on_bitfield_received(Tcp_socket * socket,const QByteArray & response,std::uint32_t payload_size) noexcept;
         void on_piece_received(Tcp_socket * socket,const QByteArray & response) noexcept;
         void on_allowed_fast_received(Tcp_socket * socket,std::uint32_t allowed_piece_idx) noexcept;
         void on_piece_downloaded(Piece & piece,std::uint32_t downloaded_piece_idx) noexcept;

         void extract_peer_response(const QByteArray & peer_response) const noexcept;
         void communicate_with_peer(Tcp_socket * socket);
         Piece_metadata get_piece_info(std::uint32_t piece_idx,std::uint32_t offset) const noexcept;
         void send_block_requests(Tcp_socket * socket,std::uint32_t piece_idx) noexcept;
         std::uint32_t get_current_target_piece() const noexcept;
         void write_to_disk(const QByteArray & piece_data,std::uint32_t received_piece_idx) noexcept;
         std::optional<std::pair<std::size_t,std::size_t>> get_file_handle_info(std::uint32_t piece_idx) const noexcept;
         static bool is_valid_response(Tcp_socket * socket,const QByteArray & response,Message_Id received_msg_id) noexcept;
         ///
         constexpr static std::string_view keep_alive_msg {"00000000"};
         constexpr static std::string_view choke_msg {"0000000100"};
         constexpr static std::string_view unchoke_msg {"0000000101"};
         constexpr static std::string_view interested_msg {"0000000102"};
         constexpr static std::string_view uninterested_msg {"0000000103"};
         constexpr static std::string_view have_all_msg {"000000010e"};
         constexpr static std::string_view have_none_msg {"000000010f"};
         constexpr static auto max_block_size = 1 << 14;
         inline static const auto reserved_bytes = QByteArray("\x00\x00\x00\x00\x00\x00\x00\x04",8).toHex();

         QSet<QUrl> active_peers_;
         QSet<std::uint32_t> remaining_pieces_;
         
         bencode::Metadata torrent_metadata_;
         std::vector<QFile *> file_handles_;
         QByteArray id_;
         QByteArray info_sha1_hash_;
         QByteArray handshake_msg_;
         QTimer acquire_piece_timer_;
         std::size_t torrent_size_ = 0;
         std::size_t piece_size_ = 0;
         std::uint32_t piece_cnt_ = 0;
         std::uint32_t spare_bitfield_bits_ = 0;
         std::uint32_t average_block_cnt_ = 0;
         QBitArray bitfield_;
         std::vector<Piece> pieces_;
         std::uint32_t active_connection_cnt_ = 0;
         std::uint32_t downloaded_piece_cnt_ = 0; // todo figure
};

inline Peer_wire_client::Peer_wire_client(bencode::Metadata metadata,std::vector<QFile *> file_handles,QByteArray peer_id,QByteArray info_sha1_hash)
         : torrent_metadata_(std::move(metadata))
         , file_handles_(std::move(file_handles))
         , id_(std::move(peer_id))
         , info_sha1_hash_(std::move(info_sha1_hash))
         , handshake_msg_(craft_handshake_message())
         , torrent_size_(torrent_metadata_.single_file ? torrent_metadata_.single_file_size : torrent_metadata_.multiple_files_size)
         , piece_size_(torrent_metadata_.piece_length)
         , piece_cnt_(static_cast<std::uint32_t>(std::ceil(static_cast<double>(torrent_size_) / static_cast<double>(piece_size_))))
         , spare_bitfield_bits_(piece_cnt_ % 8 ? 8 - piece_cnt_ % 8 : 0)
         , average_block_cnt_(static_cast<std::uint32_t>(std::ceil(static_cast<double>(piece_size_) / max_block_size)))
         , bitfield_(static_cast<std::ptrdiff_t>(piece_cnt_ + spare_bitfield_bits_))
         , pieces_(piece_cnt_)
{
         assert(!file_handles_.empty());

         remaining_pieces_.reserve(static_cast<std::ptrdiff_t>(piece_cnt_));

         for(std::uint32_t piece_idx = 0;piece_idx < piece_cnt_;++piece_idx){
                  remaining_pieces_.insert(piece_idx);
         }
}

[[nodiscard]]
inline std::uint32_t Peer_wire_client::get_current_target_piece() const noexcept {
         // todo: actually randomize the order
         assert(!remaining_pieces_.empty());
         return *remaining_pieces_.constBegin();
}