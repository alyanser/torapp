#pragma once

#include "utility.hxx"

#include <QCryptographicHash>
#include <QBitArray>
#include <QObject>
#include <QSet>
#include <QDebug>

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
	}; Q_ENUM(Message_Id);

	Peer_wire_client(bencode::Metadata torrent_metadata,QByteArray peer_id,QByteArray info_sha1_hash);

	std::shared_ptr<Peer_wire_client> bind_lifetime() noexcept;
	void do_handshake(const std::vector<QUrl> & peer_urls) const noexcept;
signals:
	void shutdown() const;
	void piece_received(std::uint32_t piece_idx) const;
private:
	struct Piece {
		std::vector<bool> requested_blocks;
		std::vector<bool> received_blocks;
		QByteArray piece;
		std::uint32_t received_block_count = 0;
	};

	static QByteArray craft_have_message(std::uint32_t piece_idx) noexcept;
	static QByteArray craft_piece_message(std::uint32_t piece_idx,std::uint32_t block_idx,const QByteArray & content) noexcept;
	static QByteArray craft_bitfield_message(const QBitArray & bitfield) noexcept;
	QByteArray craft_request_message(std::uint32_t piece_idx,std::uint32_t block_idx) const noexcept;
	QByteArray craft_cancel_message(std::uint32_t piece_idx,std::uint32_t block_idx) const noexcept;

	static std::optional<std::pair<QByteArray,QByteArray>> verify_handshake_response(Tcp_socket * socket);
	void on_socket_ready_read(Tcp_socket * socket) const noexcept;
	bool verify_hash(std::size_t piece_idx,const QByteArray & received_packet) const noexcept;

	void on_unchoke_message_received(Tcp_socket * socket) const noexcept;
	void on_have_message_received(Tcp_socket * socket,const QByteArray & response) const noexcept;
	void on_bitfield_received(Tcp_socket * socket,const QByteArray & response,std::uint32_t payload_size) const noexcept;
	void on_piece_received(const QByteArray & response) const noexcept;

	void extract_peer_response(const QByteArray & peer_response) const noexcept;
	QByteArray craft_handshake_message() const noexcept;
	void communicate_with_peer(Tcp_socket * socket) const;
	bool block_present(std::uint32_t piece_idx,std::uint32_t block_idx) const noexcept;
	std::tuple<std::uint32_t,std::uint32_t,std::uint32_t> get_piece_info(std::uint32_t piece_idx,std::uint32_t block_idx) const noexcept;
	///
	constexpr static std::string_view keep_alive_message {"00000000"};
	constexpr static std::string_view choke_message {"0000000100"};
	constexpr static std::string_view unchoke_message {"0000000101"};
	constexpr static std::string_view interested_message {"0000000102"};
	constexpr static std::string_view uninterested_message {"0000000103"};
	constexpr static auto max_block_size = 1 << 14;

	mutable QSet<QUrl> active_peers_;
	mutable std::uint64_t downloaded_piece_count_ = 0;

	bencode::Metadata  metadata_;
	QByteArray id_;
	QByteArray info_sha1_hash_;
	QByteArray handshake_message_;
	std::uint64_t torrent_size_ = 0;
	std::uint64_t piece_size_ = 0;
	std::uint64_t total_piece_count_ = 0;
	std::uint64_t spare_bitfield_bits_ = 0;
	std::uint64_t average_block_count_ = 0;
	QBitArray bitfield_;
	mutable std::vector<Piece> pieces_;
};

inline Peer_wire_client::Peer_wire_client(bencode::Metadata metadata,QByteArray peer_id,QByteArray info_sha1_hash) : 
	metadata_(std::move(metadata)), 
	id_(std::move(peer_id)), 
	info_sha1_hash_(std::move(info_sha1_hash)),
	handshake_message_(craft_handshake_message()), 
	torrent_size_(metadata_.single_file ? metadata_.single_file_size : metadata_.multiple_files_size),
	piece_size_(metadata_.piece_length),
	total_piece_count_(static_cast<std::uint64_t>(std::ceil(static_cast<double>(torrent_size_) / static_cast<double>(piece_size_)))),
	spare_bitfield_bits_(total_piece_count_ % 8 ? 8 - total_piece_count_ % 8 : 0),
	average_block_count_(static_cast<std::uint64_t>(std::ceil(static_cast<double>(piece_size_) / max_block_size))),
	bitfield_(static_cast<std::ptrdiff_t>(total_piece_count_ + spare_bitfield_bits_)),
	pieces_(total_piece_count_)
{
	for(const auto & [lhs,rhs] : metadata_.file_info){
		qInfo() << lhs.data() << rhs;
	}
}

inline std::shared_ptr<Peer_wire_client> Peer_wire_client::bind_lifetime() noexcept {
	connect(this,&Peer_wire_client::shutdown,this,[self = shared_from_this()]{},Qt::SingleShotConnection);
	return shared_from_this();
}

[[nodiscard]]
inline bool Peer_wire_client::verify_hash(const std::size_t piece_idx,const QByteArray & received_packet) const noexcept {
	constexpr auto sha1_hash_length = 20;
	assert(piece_idx < total_piece_count_ && piece_idx * sha1_hash_length < metadata_.pieces.size());

	QByteArray piece_hash(metadata_.pieces.substr(piece_idx * sha1_hash_length,sha1_hash_length).data(),sha1_hash_length);
	assert(piece_hash.size() % sha1_hash_length == 0);
	
	return piece_hash == QCryptographicHash::hash(received_packet,QCryptographicHash::Sha1);
}

inline std::tuple<std::uint32_t,std::uint32_t,std::uint32_t> Peer_wire_client::get_piece_info(const std::uint32_t piece_idx,const std::uint32_t block_idx) const noexcept {
	const auto piece_size = piece_idx == total_piece_count_ - 1 && torrent_size_ % piece_size_ ? torrent_size_ % piece_size_ : piece_size_;
	const auto total_blocks = static_cast<std::uint64_t>(std::ceil(static_cast<double>(piece_size) / max_block_size));
	
	assert(block_idx < total_blocks);

	const auto block_size = static_cast<std::uint32_t>(block_idx == total_blocks - 1 && piece_size % max_block_size ? piece_size % max_block_size : max_block_size);

	return std::make_tuple(piece_size,block_size,total_blocks);
}