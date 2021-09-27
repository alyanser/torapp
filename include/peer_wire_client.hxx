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
	static std::optional<std::pair<QByteArray,QByteArray>> handle_handshake_response(Tcp_socket * socket);
	static QByteArray craft_packet_have_message(std::uint32_t piece_index) noexcept;
	static QByteArray craft_packet_request_message(std::uint32_t index,std::uint32_t offset,std::uint32_t length) noexcept;
	static QByteArray craft_packet_cancel_message(std::uint32_t index,std::uint32_t offset,std::uint32_t length) noexcept;
	static QByteArray craft_piece_message(std::uint32_t index,std::uint32_t offset,const QByteArray & content) noexcept;
	
	void extract_peer_response(const QByteArray & peer_response) const noexcept;
	QByteArray craft_handshake_message() const noexcept;
	void communicate_with_peer(Tcp_socket * socket) const;
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
};

inline Peer_wire_client::Peer_wire_client(bencode::Metadata torrent_metadata,QByteArray peer_id,QByteArray info_sha1_hash) : 
	torrent_metadata_(std::move(torrent_metadata)), id_(std::move(peer_id)), info_sha1_hash_(std::move(info_sha1_hash)), 
	handshake_message_(craft_handshake_message())
{
	qInfo() << torrent_metadata_.pieces.size();
	qInfo() << torrent_metadata_.piece_length;
	qInfo() << torrent_metadata_.single_file_size;
	qInfo() << torrent_metadata_.multiple_files_size;
}

inline std::shared_ptr<Peer_wire_client> Peer_wire_client::bind_lifetime() noexcept {
	connect(this,&Peer_wire_client::shutdown,this,[self = shared_from_this()]{},Qt::SingleShotConnection);
	return shared_from_this();
}