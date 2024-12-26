#pragma once

#include "util.h"

#include <QTcpSocket>
#include <QBitArray>
#include <QTimer>
#include <QUrl>

class Tcp_socket : public QTcpSocket {
	Q_OBJECT
public:
	explicit Tcp_socket(QUrl peer_url, const std::int64_t uled_byte_threshold, QObject * const parent)
	    : QTcpSocket(parent),
		uled_byte_threshold(uled_byte_threshold),
		peer_url_(std::move(peer_url)) {
		configure_default_connections();
		connectToHost(QHostAddress(peer_url_.host()), static_cast<std::uint16_t>(peer_url_.port()));
		disconnect_timer_.setSingleShot(true);
	}

	std::int64_t downloaded_byte_count() const noexcept {
		return dled_byte_cnt_;
	}

	std::int64_t uploaded_byte_count() const noexcept {
		return uled_byte_cnt_;
	}

	bool is_pending_request(const util::Packet_metadata request_metadata) const noexcept {
		return pending_requests_.contains(request_metadata);
	}

	QUrl peer_url() const noexcept {
		return peer_url_;
	}

	bool remove_request(const util::Packet_metadata request_metadata) noexcept {
		return pending_requests_.remove(request_metadata);
	}

	bool request_sent(const util::Packet_metadata request_metadata) const noexcept {
		return sent_requests_.contains(request_metadata);
	}

	void reset_disconnect_timer() noexcept {
		disconnect_timer_.start(std::chrono::minutes(10));
	}

	void send_packet(const QByteArray & packet) noexcept {

		if(state() == SocketState::ConnectedState) {
			write(QByteArray::fromHex(packet));
		}
	}

	void on_peer_fault() noexcept {

		if(constexpr auto peer_fault_threshold = 50; ++peer_fault_cnt_ > peer_fault_threshold) {
			qDebug() << "well someone couldn't even implement this trivial protocol correctly";
			abort();
		}
	}

	void add_uploaded_bytes(const std::int64_t uled_byte_cnt) noexcept {
		assert(uled_byte_cnt > 0);
		uled_byte_cnt_ += uled_byte_cnt;
		emit uploaded_byte_count_changed(uled_byte_cnt);
	}

	void add_downloaded_bytes(const std::int64_t dled_byte_cnt) noexcept {
		assert(dled_byte_cnt > 0);
		dled_byte_cnt_ += dled_byte_cnt;
		emit downloaded_byte_count_changed(dled_byte_cnt_);
	}

	bool is_good_ratio() const noexcept {
		constexpr auto min_ratio = 1;
		assert(uled_byte_cnt_ >= 0 && dled_byte_cnt_ >= 0);
		return uled_byte_cnt_ <= uled_byte_threshold ? true : static_cast<double>(dled_byte_cnt_) / static_cast<double>(uled_byte_cnt_) >= min_ratio;
	}

	std::optional<QByteArray> receive_packet() noexcept;
	void post_request(util::Packet_metadata request, QByteArray packet) noexcept;
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