#pragma once

#include "utility.hxx"

#include <QCryptographicHash>
#include <QBitArray>
#include <QObject>
#include <QTimer>
#include <QSet>
#include <random>

class Tcp_socket;

class Peer_wire_client : public QObject, public std::enable_shared_from_this<Peer_wire_client> {
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
		Cancel
	}; 

	Q_ENUM(Message_Id);

	Peer_wire_client(bencode::Metadata torrent_metadata,QByteArray peer_id,QByteArray info_sha1_hash);

	std::shared_ptr<Peer_wire_client> bind_lifetime() noexcept;
	void do_handshake(const std::vector<QUrl> & peer_urls) noexcept;
signals:
	void shutdown() const;
	void piece_downloaded(std::uint32_t piece_idx) const;
private:
	struct Piece {
		std::vector<std::uint8_t> requested_blocks;
		std::vector<bool> received_blocks;
		QByteArray piece;
		std::uint32_t received_block_count = 0;
	};

	struct Piece_metadata {
		std::uint32_t piece_size;
		std::uint32_t block_size;
		std::uint32_t total_blocks;
	};

	static QByteArray craft_have_message(std::uint32_t piece_idx) noexcept;
	static QByteArray craft_piece_message(std::uint32_t piece_idx,std::uint32_t block_idx,const QByteArray & content) noexcept;
	static QByteArray craft_bitfield_message(const QBitArray & bitfield) noexcept;
	QByteArray craft_request_message(std::uint32_t piece_idx,std::uint32_t block_idx) const noexcept;
	QByteArray craft_cancel_message(std::uint32_t piece_idx,std::uint32_t block_idx) const noexcept;

	static std::optional<std::pair<QByteArray,QByteArray>> verify_handshake_response(Tcp_socket * socket);
	void on_socket_ready_read(Tcp_socket * socket) noexcept;
	bool verify_hash(std::size_t piece_idx,const QByteArray & received_packet) const noexcept;

	void on_unchoke_message_received(Tcp_socket * socket) noexcept;
	void on_have_message_received(Tcp_socket * socket,const QByteArray & response) noexcept;
	void on_bitfield_received(Tcp_socket * socket,const QByteArray & response,std::uint32_t payload_size) noexcept;
	void on_piece_received(Tcp_socket * socket,const QByteArray & response) noexcept;

	void extract_peer_response(const QByteArray & peer_response) const noexcept;
	QByteArray craft_handshake_message() const noexcept;
	void communicate_with_peer(Tcp_socket * socket);
	Piece_metadata get_piece_info(std::uint32_t piece_idx,std::uint32_t block_idx) const noexcept;
	void send_block_requests(Tcp_socket * socket,std::uint32_t piece_idx) noexcept;
	std::uint32_t get_current_target_piece() const noexcept;
	///
	constexpr static std::string_view keep_alive_message {"00000000"};
	constexpr static std::string_view choke_message {"0000000100"};
	constexpr static std::string_view unchoke_message {"0000000101"};
	constexpr static std::string_view interested_message {"0000000102"};
	constexpr static std::string_view uninterested_message {"0000000103"};
	constexpr static std::uint64_t extension_bytes {0x80000000}; // fast extension
	constexpr static auto max_block_size = 1 << 14;

	QSet<QUrl> active_peers_;
	QSet<std::uint32_t> remaining_pieces_;
	bencode::Metadata metadata_;
	QByteArray id_;
	QByteArray info_sha1_hash_;
	QByteArray handshake_message_;
	QTimer acquire_piece_timer_;
	std::uint64_t torrent_size_ = 0;
	std::uint64_t piece_size_ = 0;
	std::uint64_t total_piece_count_ = 0;
	std::uint64_t spare_bitfield_bits_ = 0;
	std::uint64_t average_block_count_ = 0;
	QBitArray bitfield_;
	std::vector<Piece> pieces_;
};

inline Peer_wire_client::Peer_wire_client(bencode::Metadata metadata,QByteArray peer_id,QByteArray info_sha1_hash)
	: metadata_(std::move(metadata))
	, id_(std::move(peer_id))
	, info_sha1_hash_(std::move(info_sha1_hash))
	, handshake_message_(craft_handshake_message())
	, torrent_size_(metadata_.single_file ? metadata_.single_file_size : metadata_.multiple_files_size)
	, piece_size_(metadata_.piece_length)
	, total_piece_count_(static_cast<std::uint64_t>(std::ceil(static_cast<double>(torrent_size_) / static_cast<double>(piece_size_))))
	, spare_bitfield_bits_(total_piece_count_ % 8 ? 8 - total_piece_count_ % 8 : 0)
	, average_block_count_(static_cast<std::uint64_t>(std::ceil(static_cast<double>(piece_size_) / max_block_size)))
	, bitfield_(static_cast<std::ptrdiff_t>(total_piece_count_ + spare_bitfield_bits_))
	, pieces_(total_piece_count_)
{
	remaining_pieces_.reserve(static_cast<std::ptrdiff_t>(total_piece_count_));

	for(std::uint32_t piece_idx = 0;piece_idx < total_piece_count_;++piece_idx){
		remaining_pieces_.insert(piece_idx);
	}

	assert(static_cast<std::uint64_t>(remaining_pieces_.size()) == total_piece_count_);
}

inline std::shared_ptr<Peer_wire_client> Peer_wire_client::bind_lifetime() noexcept {
	connect(this,&Peer_wire_client::shutdown,this,[self = shared_from_this()]{},Qt::SingleShotConnection);
	return shared_from_this();
}

[[nodiscard]]
inline std::uint32_t Peer_wire_client::get_current_target_piece() const noexcept {
	assert(!remaining_pieces_.empty());
	return *remaining_pieces_.constBegin();
}