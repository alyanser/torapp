#pragma once

#include "util.hxx"

#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

class Tcp_socket : public QTcpSocket {
         Q_OBJECT
public:
         explicit Tcp_socket(QUrl peer_url,QObject * parent = nullptr);

         bool is_good_ratio() const noexcept;
         std::int64_t downloaded_byte_count() const noexcept;
         std::int64_t uploaded_byte_count() const noexcept;
         std::optional<QByteArray> receive_packet() noexcept;
         void reset_disconnect_timer() noexcept;
         void send_packet(const QByteArray & packet) noexcept;
         QUrl peer_url() const noexcept;
         void on_invalid_peer_reply() noexcept;
         void add_uploaded_bytes(std::int64_t uled_byte_cnt) noexcept;
         void add_downloaded_bytes(std::int64_t dled_byte_cnt) noexcept;
         ///
         QBitArray peer_bitfield;
         QByteArray peer_id;
         QSet<std::int32_t> peer_allowed_fast_set;
         QSet<std::int32_t> allowed_fast_set;
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
         QTimer disconnect_timer_;
         QUrl peer_url_;
         std::int64_t dled_byte_cnt_ = 0;
         std::int64_t uled_byte_cnt_ = 0;
         std::int8_t peer_error_cnt_ = 0;
};

[[nodiscard]]
inline QUrl Tcp_socket::peer_url() const noexcept {
         return peer_url_;
}

inline void Tcp_socket::on_invalid_peer_reply() noexcept {
         
         if(constexpr auto peer_error_threshold = 5;++peer_error_cnt_ > peer_error_threshold){
                  qDebug() << "Peer made too many mistakes. aborting";
                  abort();
         }
}

[[nodiscard]]
inline bool Tcp_socket::is_good_ratio() const noexcept {
         constexpr auto min_ratio = 0.25;
         // todo: make the threshold == piece_size
         constexpr auto uled_byte_threshold = 2097152; // 1 mb
         assert(uled_byte_cnt_ >= 0 && dled_byte_cnt_ >= 0);
         return !uled_byte_cnt_ || static_cast<double>(dled_byte_cnt_) / static_cast<double>(uled_byte_cnt_) >= min_ratio ? true : uled_byte_cnt_ <= uled_byte_threshold;
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

inline void Tcp_socket::reset_disconnect_timer() noexcept {
         disconnect_timer_.start(std::chrono::minutes(10));
}