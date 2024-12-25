#pragma once

#include "torrent_properties_displayer.h"
#include "util.h"

#include <bencode_parser.h>
#include <QBitArray>
#include <QObject>
#include <QTimer>
#include <QSet>

namespace magnet {

struct Metadata;

}

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
		Allowed_Fast,
		Extended_Protocol = 20
	};

	Q_ENUM(Message_Id);

	enum State {
		Verification,
		Leecher,
		Seed
	};

	Q_ENUM(State);

	enum Metadata_Id {
		Request,
		Data,
		Reject
	};

	Q_ENUM(Metadata_Id);

	Peer_wire_client(bencode::Metadata torrent_metadata, util::Download_resources resources, QByteArray id, QByteArray info_sha1_hash);
	Peer_wire_client(magnet::Metadata torrent_metadata, util::Download_resources resources, QByteArray id);

	std::int64_t downloaded_byte_count() const noexcept {
		return dled_byte_cnt_;
	}

	std::int64_t uploaded_byte_count() const noexcept {
		return uled_byte_cnt_;
	}

	std::int64_t remaining_byte_count() const noexcept {
		return total_byte_cnt_ - dled_byte_cnt_;
	}

	void connect_to_peers(const QList<QUrl> & peer_urls) noexcept;
signals:
	void piece_verified(std::int32_t piece_idx) const;
	void existing_pieces_verified() const;
	void download_finished() const;
	void send_requests() const;
	void request_rejected(util::Packet_metadata request_metadata) const;
	void valid_block_received(util::Packet_metadata packet_metadata) const;
	void metadata_received() const;
	void new_download_requested(QString dl_path, bencode::Metadata torrent_metadata, QByteArray info_sha1_hash) const;

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

	template<Message_Id message_id>
	static QByteArray craft_generic_message(util::Packet_metadata packet_metadata) noexcept;

	static QByteArray craft_have_message(std::int32_t piece_idx) noexcept;
	static QByteArray craft_piece_message(const QByteArray & piece_data, std::int32_t piece_idx, std::int32_t piece_offset) noexcept;
	static QByteArray craft_bitfield_message(const QBitArray & bitfield) noexcept;
	static QByteArray craft_allowed_fast_message(std::int32_t piece_idx) noexcept;
	static QByteArray craft_metadata_request(std::int64_t block_idx, std::int8_t peer_ut_metadata_idx) noexcept;
	QByteArray craft_handshake_message() const noexcept;

	void on_socket_ready_read(Tcp_socket * socket) noexcept;
	void on_have_message_received(Tcp_socket * socket, std::int32_t peer_have_piece_idx) noexcept;
	void on_bitfield_received(Tcp_socket * socket) noexcept;
	void on_block_received(Tcp_socket * socket, const QByteArray & reply) noexcept;
	void on_allowed_fast_received(Tcp_socket * socket, std::int32_t allowed_piece_idx) noexcept;
	void on_piece_downloaded(Piece & dled_piece, std::int32_t dled_piece_idx) noexcept;
	void on_block_request_received(Tcp_socket * socket, const QByteArray & request) noexcept;
	void on_suggest_piece_received(Tcp_socket * socket, std::int32_t suggested_piece_idx) noexcept;
	void on_socket_connected(Tcp_socket * socket) noexcept;
	void on_handshake_reply_received(Tcp_socket * socket, const QByteArray & reply);
	void on_piece_verified(std::int32_t verified_piece_idx) noexcept;
	void send_block_requests(Tcp_socket * socket, std::int32_t piece_idx) noexcept;
	void on_extension_message_received(Tcp_socket * socket, const QByteArray & message);
	void on_extension_handshake_received(Tcp_socket * socket, const QByteArray & message);
	void on_extension_metadata_message_received(Tcp_socket * socket, const QByteArray & message);
	void send_metadata_requests(Tcp_socket * socket) const noexcept;

	qsizetype file_size(qsizetype file_idx) const noexcept {
		return static_cast<qsizetype>(torrent_metadata_.file_info[static_cast<std::size_t>(file_idx)].second);
	}

	bool is_valid_piece_index(std::int32_t piece_idx) const noexcept {
		return piece_idx >= 0 && piece_idx < total_piece_cnt_;
	}

	std::optional<std::pair<QByteArray, QByteArray>> verify_handshake_reply(Tcp_socket * socket, const QByteArray & reply) const noexcept;
	void verify_existing_pieces() noexcept;
	bool verify_piece_hash(const QByteArray & received_piece, std::int32_t piece_idx) const noexcept;
	bool validate_metadata_piece_info(std::int64_t piece_idx, std::int64_t received_raw_dict_size) const noexcept;

	static util::Packet_metadata extract_packet_metadata(const QByteArray & reply);
	void communicate_with_peer(Tcp_socket * socket);
	Piece_metadata piece_info(std::int32_t piece_idx, std::int32_t piece_offset = 0) const noexcept;

	bool write_to_disk(const QByteArray & received_piece, std::int32_t received_piece_idx) noexcept;
	std::optional<QByteArray> read_from_disk(std::int32_t requested_piece_idx) noexcept;

	void write_settings() const noexcept;
	void read_settings() noexcept;

	static bool is_valid_reply(Tcp_socket * socket, const QByteArray & reply, Message_Id received_msg_id) noexcept;

	std::optional<std::pair<qsizetype, qsizetype>> beginning_file_handle_info(std::int32_t piece_idx) const noexcept;
	std::int32_t piece_size(std::int32_t piece_idx) const noexcept;

	static QSet<std::int32_t> generate_allowed_fast_set(std::uint32_t peer_ip, std::int32_t total_piece_cnt) noexcept;
	void clear_piece(std::int32_t piece_idx) noexcept;
	void configure_default_connections() noexcept;
	void fill_target_piece_indexes() noexcept;
	///
	constexpr static std::string_view protocol_tag{"BitTorrent protocol"};
	constexpr static std::string_view keep_alive_msg{"00000000"};
	constexpr static std::string_view choke_msg{"0000000100"};
	constexpr static std::string_view unchoke_msg{"0000000101"};
	constexpr static std::string_view interested_msg{"0000000102"};
	constexpr static std::string_view uninterested_msg{"0000000103"};
	constexpr static std::string_view have_all_msg{"000000010e"};
	constexpr static std::string_view have_none_msg{"000000010f"};
	constexpr static std::string_view reserved_bytes{"0000000000100004"};
	constexpr static std::string_view extended_handshake_dict{"d1:md11:ut_metadatai1eee"};
	constexpr static std::int16_t max_block_size = 1 << 14;
	QList<std::pair<QFile *, std::int64_t>> file_handles_; // {file_handle,count of bytes downloaded}
	QList<QUrl> active_peers_;
	QList<std::int32_t> target_piece_idxes_;
	Torrent_properties_displayer properties_displayer_;
	QByteArray id_;
	QByteArray info_sha1_hash_;
	QByteArray handshake_msg_;
	QByteArray raw_metadata_;
	QString dl_path_;
	QBitArray bitfield_;
	QBitArray metadata_field_;
	QTimer settings_timer_;
	QTimer request_timer_;
	bencode::Metadata torrent_metadata_;
	Download_tracker * tracker_ = nullptr;
	std::int64_t dled_byte_cnt_ = 0;
	std::int64_t uled_byte_cnt_ = 0;
	std::int64_t session_dled_byte_cnt_ = 0;
	std::int64_t session_uled_byte_cnt_ = 0;
	std::int64_t total_byte_cnt_ = 0;
	std::int64_t torrent_piece_size_ = 0;
	std::int64_t metadata_size_ = 0;
	std::int64_t total_metadata_piece_cnt_ = 0;
	std::int32_t total_piece_cnt_ = 0;
	std::int32_t spare_piece_cnt_ = 0;
	std::int32_t average_block_cnt_ = 0;
	std::int32_t dled_piece_cnt_ = 0;
	std::int32_t obtained_metadata_piece_cnt_ = 0;
	bool has_metadata_ = false;
	State state_ = State::Verification;
	QList<std::int32_t> peer_additive_bitfield_;
	QList<Piece> pieces_;
};
