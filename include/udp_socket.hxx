#pragma once

#include "util.hxx"

#include <QUdpSocket>
#include <QTimer>
#include <QUrl>

class Udp_socket : public QUdpSocket {
         Q_OBJECT
public :
         enum class State { 
                  Connect,
                  Scrape,
                  Announce
         }; 
         
         Q_ENUM(State);

         Udp_socket(QUrl url,QByteArray connect_request,QObject * parent = nullptr);

         void start_interval_timer(std::chrono::seconds interval_timeout) noexcept;
         void send_initial_request(const QByteArray & request,State state) noexcept;
         void send_request(const QByteArray & request) noexcept;
         ///
         QByteArray announce_request;
         QByteArray scrape_request;
         std::int32_t txn_id = 0;
signals:
         void connection_timed_out() const;
private:
         std::chrono::seconds get_timeout() const noexcept;
         void configure_default_connections() noexcept;
         void send_packet(const QByteArray & packet) noexcept;
         void reset_time_specs() noexcept;
         ///
         QByteArray connect_request_;
         QTimer connection_timer_;
         QTimer interval_timer_;
         std::chrono::seconds interval_time_{};
         State state_ = State::Connect;
         std::int8_t timeout_factor_ = 0;
         bool connection_id_valid_ = true;
};

inline void Udp_socket::start_interval_timer(const std::chrono::seconds interval_timeout) noexcept {
         interval_timer_.start(interval_timeout);
}

[[nodiscard]]
inline std::chrono::seconds Udp_socket::get_timeout() const noexcept {
         constexpr auto protocol_constant = 15;
         const std::chrono::seconds timeout_seconds(protocol_constant * static_cast<std::int32_t>(std::exp2(timeout_factor_)));
         return timeout_seconds;
}

inline void Udp_socket::send_request(const QByteArray & request) noexcept {
         state_ != State::Connect && !connection_id_valid_ ? send_initial_request(connect_request_,State::Connect) : send_packet(request);
}