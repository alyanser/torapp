#include "tcp_socket.h"

#include <QHostAddress>

void Tcp_socket::post_request(util::Packet_metadata request, QByteArray packet) noexcept {
	assert(!packet.isEmpty());
	assert(!pending_requests_.contains(request));

	pending_requests_[request] = std::move(packet);

	if(!request_timer.isActive()) {
		request_timer.start();
	}
}

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

void Tcp_socket::configure_default_connections() noexcept {
	connect(this, &Tcp_socket::disconnected, this, &Tcp_socket::deleteLater);
	connect(this, &Tcp_socket::readyRead, this, &Tcp_socket::reset_disconnect_timer);

	connect(this, &Tcp_socket::connected, [&disconnect_timer_ = disconnect_timer_] {
		disconnect_timer_.start(std::chrono::minutes(2));
	});

	disconnect_timer_.callOnTimeout(this, [this] {
		state() == SocketState::UnconnectedState ? deleteLater() : disconnectFromHost();
	});

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