#pragma once

#include "utility.hxx"

#include <QStackedWidget>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QTime>

namespace bencode {
         struct Metadata;
}

class Download_tracker : public QWidget {
         Q_OBJECT
public:
         enum class Error { 
                  Null,
                  File_Write,
                  Unknown_Network,
                  File_Lock,
                  Not_Enough_Space,
                  Custom
         };

         Download_tracker(const QString & dl_path,QUrl url,QWidget * parent = nullptr);
         Download_tracker(const QString & dl_path,bencode::Metadata torrent_metadata,QWidget * parent = nullptr);

         constexpr Error error() const noexcept;
         constexpr void set_downloaded_bytes_offset(std::int64_t dl_bytes_offset) noexcept;
         std::int32_t get_elapsed_seconds() const noexcept;
         void set_error_and_finish(Error new_error) noexcept;
         void set_error_and_finish(const QString & custom_error) noexcept;
         void switch_to_finished_state() noexcept;
signals:
         void retry_download(const QString & file_path,const QUrl & url) const;
         void retry_download(const QString & file_path,const bencode::Metadata & torrent_metadata) const;
         void delete_file_permanently() const;
         void move_file_to_trash() const;
         void request_satisfied() const;
         void download_dropped() const;
public slots:
         void download_progress_update(std::int64_t received_byte_cnt,std::int64_t total_byte_cnt) noexcept;
         void upload_progress_update(std::int64_t send_byte_cnt,std::int64_t total_byte_cnt) noexcept;
private:
         explicit Download_tracker(const QString & dl_path,QWidget * parent = nullptr);
         
         void configure_default_connections() noexcept;
         void setup_layout() noexcept;
         void setup_file_status_layout() noexcept;
         void setup_network_status_layout() noexcept;
         void setup_state_widget() noexcept;
         void update_error_line() noexcept;
         ///
         Error error_ {Error::Null};
         QVBoxLayout central_layout_ {this};
         QHBoxLayout file_stat_layout_;
         QHBoxLayout network_stat_layout_;

         QHBoxLayout package_name_layout_;
         QLabel package_name_buddy_ {"Name:"};
         QLabel package_name_label_;

         QHBoxLayout dl_path_layout_;
         QLabel dl_path_buddy_ {"Path:"};
         QLabel dl_path_label;

         QStackedWidget state_holder_;
         QLineEdit error_line_;
         QProgressBar dl_progress_bar_;

         QHBoxLayout dl_quanitity_layout_;
         QLabel dl_quantity_buddy_ {"Downloaded:"};
         QLabel dl_quantity_label_ {"0 byte (s) / 0 byte (s)"};

         QHBoxLayout ul_quantity_layout_;
         QLabel ul_quantity_buddy_ {"Uploaded:"};
         QLabel ul_quanitity_label_ {"0 byte (s) / 0 byte (s)"};

         QStackedWidget terminate_buttons_holder_;
         QPushButton finish_button_ {"Finish"};
         QPushButton cancel_button_ {"Cancel"};

         QStackedWidget initiate_buttons_holder_;
         QPushButton open_button_ {"Open"};
         QPushButton retry_button_ {"Retry"};

         QHBoxLayout time_elapsed_layout_;
         QTime time_elapsed_ {0,0,1}; // 1 to prevent zero division
         QTimer time_elapsed_timer_;
         QLabel time_elapsed_buddy_ {"Time elapsed:"};
         QLabel time_elapsed_label_ {time_elapsed_.toString() + " hh::mm::ss"};

         QHBoxLayout dl_speed_layout_;
         QLabel dl_speed_buddy_ {"Download Speed:"};
         QLabel dl_speed_label_ {"0 bytes/sec"};

         QPushButton delete_button_ {"Delete"};
         QPushButton open_dir_button_ {"Open directory"};

         std::int64_t dl_bytes_offset_ = 0;
};

inline void Download_tracker::setup_layout() noexcept {
         central_layout_.addLayout(&file_stat_layout_);
         central_layout_.addWidget(&state_holder_);
         central_layout_.addLayout(&network_stat_layout_);
}

[[nodiscard]]
constexpr Download_tracker::Error Download_tracker::error() const noexcept {
         return error_;
}

constexpr void Download_tracker::set_downloaded_bytes_offset(const std::int64_t dl_bytes_offset) noexcept {
         dl_bytes_offset_ = dl_bytes_offset;
}

[[nodiscard]]
inline std::int32_t Download_tracker::get_elapsed_seconds() const noexcept {
         constexpr auto secs_in_min = 60;
         constexpr auto secs_in_hr = secs_in_min * 60;
         return time_elapsed_.second() + time_elapsed_.minute() * secs_in_min + time_elapsed_.hour() * secs_in_hr;
}

inline void Download_tracker::set_error_and_finish(const Error error) noexcept {
         assert(error != Error::Null && error != Error::Custom);
         error_ = error;
         update_error_line();
         switch_to_finished_state();
}

inline void Download_tracker::set_error_and_finish(const QString & custom_error) noexcept {
         error_ = Error::Custom;
         error_line_.setText(custom_error);
         switch_to_finished_state();
}

inline void Download_tracker::upload_progress_update(const std::int64_t send_byte_cnt,const std::int64_t total_byte_cnt) noexcept {
         ul_quanitity_label_.setText(util::conversion::stringify_bytes(send_byte_cnt,total_byte_cnt));
}