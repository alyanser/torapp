#pragma once

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

         Udp_socket(const QUrl & url,QByteArray connect_request,QObject * parent);

         constexpr std::uint32_t txn_id() const noexcept;
         constexpr void set_interval_time(std::chrono::seconds interval_time) noexcept;
         constexpr std::chrono::seconds interval_time() const noexcept;

         void set_connect_request(QByteArray connect_request) noexcept;
         void set_announce_request(QByteArray announce_request) noexcept;
         void set_scrape_request(QByteArray scrape_request) noexcept;
         
         const QByteArray & connect_request() const noexcept;
         const QByteArray & announce_request() const noexcept;
         const QByteArray & scrape_request() const noexcept;

         void start_interval_timer(std::chrono::seconds interval_timeout) noexcept;
         void send_initial_request(const QByteArray & request,State new_state) noexcept;
private:
         constexpr void set_txn_id(std::uint32_t txn_id) noexcept;
         constexpr std::chrono::seconds get_timeout() const noexcept;
         void configure_default_connections() noexcept;
         void send_request(const QByteArray & request) noexcept;
         void send_packet(const QByteArray & packet) noexcept;
         void reset_time_specs() noexcept;
         ///
         QByteArray announce_request_;
         QByteArray scrape_request_;
         QTimer connection_timer_;
         QTimer interval_timer_;
         std::chrono::seconds interval_time_ {};
         State state_ = State::Connect;
         std::uint32_t txn_id_ = 0;
         std::uint8_t timeout_factor_ = 0;
         bool connection_id_valid_ = true;
         QByteArray connect_request_; // ? consider the order
};

inline Udp_socket::Udp_socket(const QUrl & url,QByteArray connect_request,QObject * const parent) 
         : QUdpSocket(parent)
         , connect_request_(std::move(connect_request))
{
         assert(!connect_request_.isEmpty());
         configure_default_connections();
         connectToHost(url.host(),static_cast<std::uint16_t>(url.port()));
}

inline void Udp_socket::reset_time_specs() noexcept {
         timeout_factor_ = 0;
         connection_timer_.start(get_timeout());
         connection_id_valid_ = true;

         {
                  constexpr std::chrono::minutes protocol_validity_timeout {1};

                  QTimer::singleShot(protocol_validity_timeout,this,[&connection_id_valid_ = connection_id_valid_]{
                           connection_id_valid_ = false;
                  });
         }
}

constexpr void Udp_socket::set_txn_id(std::uint32_t txn_id) noexcept {
         txn_id_ = txn_id;
}

[[nodiscard]]
constexpr std::uint32_t Udp_socket::txn_id() const noexcept {
         return txn_id_;
}

constexpr void Udp_socket::set_interval_time(const std::chrono::seconds interval_time) noexcept {
         interval_time_ = interval_time;
}

[[nodiscard]]
constexpr std::chrono::seconds Udp_socket::interval_time() const noexcept {
         return interval_time_;
}

[[nodiscard]]
inline const QByteArray & Udp_socket::connect_request() const noexcept {
         return connect_request_;
}

[[nodiscard]]
inline const QByteArray & Udp_socket::announce_request() const noexcept {
         return announce_request_;
}

[[nodiscard]]
inline const QByteArray & Udp_socket::scrape_request() const noexcept {
         return scrape_request_;
}

inline void Udp_socket::start_interval_timer(const std::chrono::seconds interval_timeout) noexcept {
         interval_timer_.start(interval_timeout);
}

inline void Udp_socket::send_initial_request(const QByteArray & request,const State new_state) noexcept {
         state_ = new_state;
         send_packet(request);
         reset_time_specs();
}

inline void Udp_socket::send_request(const QByteArray & request) noexcept {
         
         if(state_ != State::Connect && !connection_id_valid_){
                  send_initial_request(connect_request_,State::Connect);
         }else{
                  send_packet(request);
         }
}

inline void Udp_socket::set_connect_request(QByteArray connect_request) noexcept {
         connect_request_ = std::move(connect_request);
}

inline void Udp_socket::set_announce_request(QByteArray announce_request) noexcept {
         announce_request_ = std::move(announce_request);
}

inline void Udp_socket::set_scrape_request(QByteArray scrape_request) noexcept {
         scrape_request_ = std::move(scrape_request);
}

[[nodiscard]]
constexpr std::chrono::seconds Udp_socket::get_timeout() const noexcept {
         constexpr auto protocol_constant = 15;
         const std::chrono::seconds timeout_seconds(protocol_constant * static_cast<std::int32_t>(std::exp2(timeout_factor_)));
         assert(timeout_seconds <= std::chrono::seconds{3840});
         return timeout_seconds;
}