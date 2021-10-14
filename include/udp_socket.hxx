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
         void send_initial_request(const QByteArray & request,State new_state) noexcept;
         ///
         QByteArray announce_request;
         QByteArray scrape_request;
         std::int32_t txn_id = 0;
private:
         constexpr std::chrono::seconds get_timeout() const noexcept;
         void configure_default_connections() noexcept;
         void send_request(const QByteArray & request) noexcept;
         void send_packet(const QByteArray & packet) noexcept;
         void reset_time_specs() noexcept;
         ///
         QByteArray connect_request_;
         QTimer connection_timer_;
         QTimer interval_timer_;
         std::chrono::seconds interval_time_{};
         State state_ = State::Connect;
         std::int8_t timeout_factor_ = 0; //! 
         bool connection_id_valid_ = true;
};

inline Udp_socket::Udp_socket(const QUrl url,QByteArray connect_request,QObject * const parent) 
         : QUdpSocket(parent)
         , connect_request_(std::move(connect_request))
{
         assert(!connect_request_.isEmpty());
         configure_default_connections();
         connectToHost(url.host(),static_cast<std::uint16_t>(url.port()));
}

inline void Udp_socket::start_interval_timer(const std::chrono::seconds interval_timeout) noexcept {
         interval_timer_.start(interval_timeout);
}

inline void Udp_socket::send_initial_request(const QByteArray & request,const State new_state) noexcept {
         state_ = new_state;
         send_packet(request);
         reset_time_specs();
}

[[nodiscard]]
constexpr std::chrono::seconds Udp_socket::get_timeout() const noexcept {
         constexpr auto protocol_constant = 15;
         const std::chrono::seconds timeout_seconds(protocol_constant * static_cast<std::int32_t>(std::exp2(timeout_factor_)));
         assert(timeout_seconds <= std::chrono::seconds(3840));
         return timeout_seconds;
}

inline void Udp_socket::send_request(const QByteArray & request) noexcept {

         if(state_ != State::Connect && !connection_id_valid_){
                  send_initial_request(connect_request_,State::Connect);
         }else{
                  send_packet(request);
         }
}

inline void Udp_socket::send_packet(const QByteArray & hex_packet) noexcept {
         const auto raw_packet = QByteArray::fromHex(hex_packet);
         write(raw_packet);
         constexpr auto txn_id_offset = 12;
         txn_id = util::extract_integer<std::int32_t>(raw_packet,txn_id_offset);
}