#pragma once

#include "utility.hxx"

#include <QObject>
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
	};

	Q_ENUM(Message_Id);

	Peer_wire_client(bencode::Metadata torrent_metadata,QByteArray peer_id,QByteArray info_sha1_hash);

	std::shared_ptr<Peer_wire_client> bind_lifetime() noexcept;
	void do_handshake(const std::vector<QUrl> & peer_urls) const noexcept;
signals:
	void shutdown() const;
private:
	static std::optional<std::pair<QByteArray,QByteArray>> verify_handshake_response(Tcp_socket * socket);
	static QByteArray craft_packet_have_message(std::uint32_t piece_index) noexcept;
	static QByteArray craft_packet_request_message(std::uint32_t index,std::uint32_t offset,std::uint32_t length) noexcept;
	static QByteArray craft_packet_cancel_message(std::uint32_t index,std::uint32_t offset,std::uint32_t length) noexcept;
	static QByteArray craft_piece_message(std::uint32_t index,std::uint32_t offset,const QByteArray & content) noexcept;
	static std::uint64_t calculate_total_pieces(const bencode::Metadata & metadata) noexcept;
	
	void extract_peer_response(const QByteArray & peer_response) const noexcept;
	QByteArray craft_handshake_message() const noexcept;
	static void communicate_with_peer(Tcp_socket * socket);
	///
	static constexpr std::string_view keep_alive_message_ {"00000000"};
	static constexpr std::string_view choke_message_ {"0000000100"};
	static constexpr std::string_view unchoke_message_ {"0000000101"};
	static constexpr std::string_view interested_message_ {"0000000102"};
	static constexpr std::string_view uninterested_message_ {"0000000103"};
	
	bencode::Metadata torrent_metadata_;
	QByteArray id_;
	QByteArray info_sha1_hash_;
	QByteArray handshake_message_;
	std::uint64_t total_pieces_ = 0;
};

inline Peer_wire_client::Peer_wire_client(bencode::Metadata torrent_metadata,QByteArray peer_id,QByteArray info_sha1_hash) : 
	torrent_metadata_(std::move(torrent_metadata)), id_(std::move(peer_id)), info_sha1_hash_(std::move(info_sha1_hash)),
	handshake_message_(craft_handshake_message()), total_pieces_(calculate_total_pieces(torrent_metadata))
{
	assert(total_pieces_);
}

inline std::shared_ptr<Peer_wire_client> Peer_wire_client::bind_lifetime() noexcept {
	connect(this,&Peer_wire_client::shutdown,this,[self = shared_from_this()]{},Qt::SingleShotConnection);
	return shared_from_this();
}

inline std::uint64_t Peer_wire_client::calculate_total_pieces(const bencode::Metadata & metadata) noexcept {
	const auto torrent_size = metadata.single_file ? metadata.single_file_size : metadata.multiple_files_size;
	assert(metadata.piece_length && torrent_size);
	
	return static_cast<std::uint64_t>(std::ceil(static_cast<double>(torrent_size) / static_cast<double>(metadata.piece_length)));
}