#include "tcp_socket.h"

#include <QHostAddress>

Tcp_socket::Tcp_socket(QUrl peer_url, const std::int64_t uled_byte_threshold, QObject * const parent)
    : QTcpSocket(parent), uled_byte_threshold(uled_byte_threshold), peer_url_(std::move(peer_url)) {
	configure_default_connections();
	connectToHost(QHostAddress(peer_url_.host()), static_cast<std::uint16_t>(peer_url_.port()));

	disconnect_timer_.setSingleShot(true);
}

[[nodiscard]]
QUrl Tcp_socket::peer_url() const noexcept {
	return peer_url_;
}

void Tcp_socket::on_peer_fault() noexcept {

	if(constexpr auto peer_fault_threshold = 50; ++peer_fault_cnt_ > peer_fault_threshold) {
		qDebug() << "well someone couldn't even implement this trivial protocol correctly";
		abort();
	}
}

[[nodiscard]]
bool Tcp_socket::is_pending_request(util::Packet_metadata request) const noexcept {
	return pending_requests_.contains(request);
}

[[nodiscard]]
std::int64_t Tcp_socket::downloaded_byte_count() const noexcept {
	return dled_byte_cnt_;
}

[[nodiscard]]
std::int64_t Tcp_socket::uploaded_byte_count() const noexcept {
	return uled_byte_cnt_;
}

void Tcp_socket::add_uploaded_bytes(const std::int64_t uled_byte_cnt) noexcept {
	assert(uled_byte_cnt > 0);
	uled_byte_cnt_ += uled_byte_cnt;
	emit uploaded_byte_count_changed(uled_byte_cnt);
}

void Tcp_socket::add_downloaded_bytes(const std::int64_t dled_byte_cnt) noexcept {
	assert(dled_byte_cnt > 0);
	dled_byte_cnt_ += dled_byte_cnt;
	emit downloaded_byte_count_changed(dled_byte_cnt_);
}

void Tcp_socket::post_request(util::Packet_metadata request, QByteArray packet) noexcept {
	assert(!packet.isEmpty());
	assert(!pending_requests_.contains(request));

	pending_requests_[request] = std::move(packet);

	if(!request_timer.isActive()) {
		request_timer.start();
	}
}

bool Tcp_socket::remove_request(const util::Packet_metadata request) noexcept { return pending_requests_.remove(request); }

[[nodiscard]]
bool Tcp_socket::request_sent(const util::Packet_metadata request_metadata) const noexcept {
	return sent_requests_.contains(request_metadata);
}

void Tcp_socket::reset_disconnect_timer() noexcept { disconnect_timer_.start(std::chrono::minutes(10)); }

[[nodiscard]]
std::optional<QByteArray> Tcp_socket::receive_packet() noexcept {
	auto & msg_size = receive_buffer_.first;

	if(!handshake_done && !msg_size) {
		constexpr auto protocol_handshake_msg_size = 68;
		msg_size = protocol_handshake_msg_size;
	}

	if(!msg_size) {
		constexpr auto msg_len_byte_cnt = 4;

		if(bytesAvailable() < msg_len_byte_cnt) {
			return {};
		}

		startTransaction();
		const auto size_buffer = read(msg_len_byte_cnt);
		assert(size_buffer.size() == msg_len_byte_cnt);

		try {
			msg_size = util::extract_integer<std::int32_t>(size_buffer);
		} catch(const std::exception & exception) {
			qDebug() << exception.what();
			rollbackTransaction();
			assert(!msg_size);
			return {};
		}

		commitTransaction();
	}

	assert(msg_size);

	if(!*msg_size) { // keep alive packet
		qDebug() << "Keep alive packet";
		msg_size.reset();
		return {};
	}

	auto & buffer_data = receive_buffer_.second;

	if(!buffer_data.capacity()) {
		buffer_data.reserve(*msg_size);
	}

	if(buffer_data += read(*msg_size - buffer_data.size()); buffer_data.size() == *msg_size) {
		assert(!buffer_data.isEmpty());
		msg_size.reset();
		return std::move(buffer_data);
	}

	assert(buffer_data.size() < *msg_size);
	return {};
}

[[nodiscard]]
bool Tcp_socket::is_good_ratio() const noexcept {
	constexpr auto min_ratio = 1;
	assert(uled_byte_cnt_ >= 0 && dled_byte_cnt_ >= 0);
	return uled_byte_cnt_ <= uled_byte_threshold ? true : static_cast<double>(dled_byte_cnt_) / static_cast<double>(uled_byte_cnt_) >= min_ratio;
}

void Tcp_socket::send_packet(const QByteArray & packet) noexcept {
	assert(!packet.isEmpty());

	if(state() == SocketState::ConnectedState) {
		write(QByteArray::fromHex(packet));
	}
}

void Tcp_socket::configure_default_connections() noexcept {
	connect(this, &Tcp_socket::disconnected, this, &Tcp_socket::deleteLater);
	connect(this, &Tcp_socket::readyRead, this, &Tcp_socket::reset_disconnect_timer);

	connect(this, &Tcp_socket::connected, [&disconnect_timer_ = disconnect_timer_] { disconnect_timer_.start(std::chrono::minutes(2)); });

	disconnect_timer_.callOnTimeout(this, [this] { state() == SocketState::UnconnectedState ? deleteLater() : disconnectFromHost(); });

	request_timer.callOnTimeout(this, [this] {
		if(pending_requests_.isEmpty()) {
			return request_timer.stop();
		}

		const auto & [request_metadata, packet] = *pending_requests_.constKeyValueBegin();
		send_packet(packet);
		sent_requests_.insert(request_metadata);
		pending_requests_.erase(pending_requests_.constBegin());
	});
}