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
		Cancel,
	}; Q_ENUM(Message_Id);

	Peer_wire_client(bencode::Metadata torrent_metadata,QByteArray peer_id,QByteArray info_sha1_hash);

	std::shared_ptr<Peer_wire_client> bind_lifetime() noexcept;
	void do_handshake(const std::vector<QUrl> & peer_urls) const noexcept;
signals:
	void shutdown() const;
private:
	static QByteArray craft_have_message(std::uint32_t piece_index) noexcept;
	static QByteArray craft_request_message(std::uint32_t index,std::uint32_t offset,std::uint32_t length) noexcept;
	static QByteArray craft_cancel_message(std::uint32_t index,std::uint32_t offset,std::uint32_t length) noexcept;
	static QByteArray craft_piece_message(std::uint32_t index,std::uint32_t offset,const QByteArray & content) noexcept;
	static QByteArray craft_bitfield_message(const QBitArray & bitfield) noexcept;

	static std::size_t calculate_total_pieces(const bencode::Metadata & metadata) noexcept;
	static std::optional<std::pair<QByteArray,QByteArray>> verify_handshake_response(Tcp_socket * socket);

	bool verify_hash(std::size_t piece_index,const QByteArray & received_packet) const noexcept;

	void extract_peer_response(const QByteArray & peer_response) const noexcept;
	QByteArray craft_handshake_message() const noexcept;
	void communicate_with_peer(Tcp_socket * socket) const;
	///
	constexpr static std::string_view keep_alive_message {"00000000"};
	constexpr static std::string_view choke_message {"0000000100"};
	constexpr static std::string_view unchoke_message {"0000000101"};
	constexpr static std::string_view interested_message {"0000000102"};
	constexpr static std::string_view uninterested_message {"0000000103"};

	std::uint64_t downloaded_pieces_count_ = 0;
	mutable QSet<QUrl> active_peers_;

	bencode::Metadata torrent_metadata_;
	QByteArray id_;
	QByteArray info_sha1_hash_;
	QByteArray handshake_message_;
	std::uint64_t total_pieces_ = 0;
	std::uint64_t pieces_bits_ = 0;
	std::uint64_t spare_bitfield_bits_ = 0;
	QBitArray bitfield_;
};

inline Peer_wire_client::Peer_wire_client(bencode::Metadata torrent_metadata,QByteArray peer_id,QByteArray info_sha1_hash) : 
	torrent_metadata_(std::move(torrent_metadata)), 
	id_(std::move(peer_id)), 
	info_sha1_hash_(std::move(info_sha1_hash)),
	handshake_message_(craft_handshake_message()), 
	total_pieces_(calculate_total_pieces(torrent_metadata)), 
	pieces_bits_(total_pieces_),
	spare_bitfield_bits_(pieces_bits_ % 8 ? 8 - pieces_bits_ % 8 : 0),
	bitfield_(static_cast<std::ptrdiff_t>(pieces_bits_ + spare_bitfield_bits_))
{
	assert(pieces_bits_);
	assert(total_pieces_);
}

inline std::shared_ptr<Peer_wire_client> Peer_wire_client::bind_lifetime() noexcept {
	connect(this,&Peer_wire_client::shutdown,this,[self = shared_from_this()]{},Qt::SingleShotConnection);
	return shared_from_this();
}

inline std::size_t Peer_wire_client::calculate_total_pieces(const bencode::Metadata & metadata) noexcept {
	const auto torrent_size = metadata.single_file ? metadata.single_file_size : metadata.multiple_files_size;
	assert(metadata.piece_length && torrent_size);
	return static_cast<std::size_t>(std::ceil(static_cast<double>(torrent_size) / static_cast<double>(metadata.piece_length)));
}

[[nodiscard]]
inline bool Peer_wire_client::verify_hash(const std::size_t piece_index,const QByteArray & received_packet) const noexcept {
	assert(piece_index < total_pieces_);
	constexpr auto hash_length = 20;
	assert(piece_index * hash_length < torrent_metadata_.pieces.size());

	const auto piece_hash = QByteArray(torrent_metadata_.pieces.substr(piece_index * hash_length,hash_length).data(),hash_length);
	return piece_hash == QCryptographicHash::hash(received_packet,QCryptographicHash::Sha1);
}