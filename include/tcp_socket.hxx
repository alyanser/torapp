#pragma once

#include "util.hxx"

#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <set>

class Tcp_socket : public QTcpSocket {
         Q_OBJECT
public:
         struct Request {
                  bool operator == (const Request & rhs) const noexcept;

                  std::int32_t piece_idx = 0;
                  std::int32_t piece_offset = 0;
                  std::int32_t byte_cnt = 0;
         };

         explicit Tcp_socket(QUrl peer_url,std::int64_t uled_byte_threshold,QObject * parent = nullptr);

         bool is_good_ratio() const noexcept;
         bool is_pending_request(Request request) const noexcept;
         std::int64_t downloaded_byte_count() const noexcept;
         std::int64_t uploaded_byte_count() const noexcept;
         std::optional<QByteArray> receive_packet() noexcept;
         void reset_disconnect_timer() noexcept;
         void send_packet(const QByteArray & packet) noexcept;
         QUrl peer_url() const noexcept;
         void on_peer_fault() noexcept;
         void add_uploaded_bytes(std::int64_t uled_byte_cnt) noexcept;
         void add_downloaded_bytes(std::int64_t dled_byte_cnt) noexcept;
         void post_request(Request request,QByteArray packet) noexcept;
         bool remove_request(Request request) noexcept;
         ///
         QBitArray peer_bitfield;
         QByteArray peer_id;
         QSet<std::int32_t> peer_allowed_fast_set;
         QSet<std::int32_t> allowed_fast_set;
         QTimer request_timer_;
         std::int64_t uled_byte_threshold_ = 0;
         bool handshake_done = false;
         bool am_choking = true;
         bool peer_choked = true;
         bool am_interested = false;
         bool peer_interested = false;
         bool fast_extension_enabled = false;
signals:
         void got_choked() const;
         void uploaded_byte_count_changed(std::int64_t uled_byte_cnt) const;
         void downloaded_byte_count_changed(std::int64_t uled_byte_cnt) const;
private:

         void configure_default_connections() noexcept;
         ///
         std::pair<std::optional<std::int32_t>,QByteArray> receive_buffer_;
         QHash<Request,QByteArray> requests_;
         QTimer disconnect_timer_;
         QUrl peer_url_;
         std::int64_t dled_byte_cnt_ = 0;
         std::int64_t uled_byte_cnt_ = 0;
         std::int8_t peer_fault_cnt_ = 0;
};

[[nodiscard]]
inline QUrl Tcp_socket::peer_url() const noexcept {
         return peer_url_;
}

inline void Tcp_socket::on_peer_fault() noexcept {
         
         if(constexpr auto peer_fault_threshold = 5;++peer_fault_cnt_ > peer_fault_threshold){
                  qDebug() << "Peer made too many mistakes. aborting";
                  abort();
         }
}

[[nodiscard]]
inline bool Tcp_socket::is_pending_request(Request request) const noexcept {
         return requests_.contains(request);
}

[[nodiscard]]
inline std::int64_t Tcp_socket::downloaded_byte_count() const noexcept {
         return dled_byte_cnt_;
}

[[nodiscard]]
inline std::int64_t Tcp_socket::uploaded_byte_count() const noexcept {
         return uled_byte_cnt_;
}

inline void Tcp_socket::add_uploaded_bytes(const std::int64_t uled_byte_cnt) noexcept {
         assert(uled_byte_cnt > 0);
         uled_byte_cnt_ += uled_byte_cnt;
         emit uploaded_byte_count_changed(uled_byte_cnt);
}

inline void Tcp_socket::add_downloaded_bytes(const std::int64_t dled_byte_cnt) noexcept {
         assert(dled_byte_cnt > 0);
         dled_byte_cnt_ += dled_byte_cnt;
         emit downloaded_byte_count_changed(dled_byte_cnt_);
}

inline void Tcp_socket::post_request(Request request,QByteArray packet) noexcept {
         assert(!packet.isEmpty());
         assert(!requests_.contains(request));
         
         requests_[request] = std::move(packet);

         if(!request_timer_.isActive()){
                  request_timer_.start();
         }
}

inline bool Tcp_socket::remove_request(const Request request) noexcept {
         return requests_.remove(request);
}

inline void Tcp_socket::reset_disconnect_timer() noexcept {
         disconnect_timer_.start(std::chrono::minutes(10));
}

[[nodiscard]]
inline bool Tcp_socket::Request::operator == (const Request & rhs) const noexcept {
         return byte_cnt == rhs.byte_cnt && piece_idx == rhs.piece_idx && piece_offset == rhs.piece_offset;
}

[[nodiscard]]
inline std::size_t qHash(const Tcp_socket::Request & request,const std::size_t seed = 0) noexcept {
         return qHash(request.byte_cnt,seed) ^ qHash(request.piece_idx,seed) ^ qHash(request.piece_offset,seed);
}