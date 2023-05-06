#pragma once

#include "util.h"

#include <QUdpSocket>
#include <QTimer>
#include <QUrl>

class Udp_socket : public QUdpSocket {
	Q_OBJECT
    public:
	enum class State { Connect, Scrape, Announce };

	Q_ENUM(State);

	Udp_socket(QUrl url, QByteArray connect_request, QObject * parent = nullptr);

	std::int32_t transaction_id() const noexcept;
	void start_interval_timer(std::chrono::seconds interval_timeout) noexcept;
	void send_initial_request(const QByteArray & request, State state) noexcept;
	void send_request(const QByteArray & request) noexcept;
	void set_requests(QByteArray announce_request, QByteArray scrape_request) noexcept;
	const QByteArray & announce_request() const noexcept;
	const QByteArray & scrape_request() const noexcept;
    signals:
	void connection_timed_out() const;

    private:
	std::chrono::seconds get_timeout() const noexcept;
	void configure_default_connections() noexcept;
	void send_packet(const QByteArray & packet) noexcept;
	void reset_time_specs() noexcept;
	///
	QByteArray connect_request_;
	QByteArray announce_request_;
	QByteArray scrape_request_;
	QTimer connection_timer_;
	QTimer interval_timer_;
	std::chrono::seconds interval_time_{};
	State state_ = State::Connect;
	std::int32_t txn_id_ = 0;
	std::int8_t timeout_factor_ = 0;
	bool connection_id_valid_ = true;
};