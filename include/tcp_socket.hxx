#pragma once

#include "util.hxx"

#include <QTcpSocket>
#include <QBitArray>
#include <QTimer>
#include <QUrl>

class Tcp_socket : public QTcpSocket {
	Q_OBJECT
    public:
	explicit Tcp_socket(QUrl peer_url, std::int64_t uled_byte_threshold, QObject * parent = nullptr);

	bool is_good_ratio() const noexcept;
	bool is_pending_request(util::Packet_metadata request_metadata) const noexcept;
	std::int64_t downloaded_byte_count() const noexcept;
	std::int64_t uploaded_byte_count() const noexcept;
	std::optional<QByteArray> receive_packet() noexcept;
	void reset_disconnect_timer() noexcept;
	void send_packet(const QByteArray & packet) noexcept;
	QUrl peer_url() const noexcept;
	void on_peer_fault() noexcept;
	void add_uploaded_bytes(std::int64_t uled_byte_cnt) noexcept;
	void add_downloaded_bytes(std::int64_t dled_byte_cnt) noexcept;
	void post_request(util::Packet_metadata request, QByteArray packet) noexcept;
	bool remove_request(util::Packet_metadata request_metadata) noexcept;
	bool request_sent(util::Packet_metadata request_metadata) const noexcept;
	///
	QBitArray peer_bitfield;
	QByteArray peer_id;
	QSet<std::int32_t> peer_allowed_fast_set;
	QSet<std::int32_t> allowed_fast_set;
	QSet<util::Packet_metadata> rejected_requests;
	QTimer request_timer;
	std::int64_t uled_byte_threshold = 0;
	std::int64_t peer_ut_metadata_id = -1;
	bool handshake_done = false;
	bool am_choking = true;
	bool peer_choked = true;
	bool am_interested = false;
	bool peer_interested = false;
	bool fast_extension_enabled = false;
	bool extension_protocol_enabled = false;
    signals:
	void got_choked() const;
	void uploaded_byte_count_changed(std::int64_t uled_byte_cnt) const;
	void downloaded_byte_count_changed(std::int64_t uled_byte_cnt) const;

    private:
	void configure_default_connections() noexcept;
	///
	std::pair<std::optional<std::int32_t>, QByteArray> receive_buffer_;
	QHash<util::Packet_metadata, QByteArray> pending_requests_;
	QSet<util::Packet_metadata> sent_requests_;
	QTimer disconnect_timer_;
	QUrl peer_url_;
	std::int64_t dled_byte_cnt_ = 0;
	std::int64_t uled_byte_cnt_ = 0;
	std::int8_t peer_fault_cnt_ = 0;
};