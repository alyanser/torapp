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

         Download_tracker(const QString & download_path,QUrl url,QWidget * parent = nullptr);
         Download_tracker(const QString & download_path,bencode::Metadata torrent_metadata,QWidget * parent = nullptr);

         constexpr Error error() const noexcept;
         std::uint32_t get_elapsed_seconds() const noexcept;
         void set_error_and_finish(Error new_error) noexcept;
         void set_error_and_finish(const QString & custom_error) noexcept;
         void switch_to_finished_state() noexcept;
signals:
         void retry_download(const QString & file_path,const QUrl & url) const;
         void retry_download(const QString & file_path,const bencode::Metadata & torrent_metadata) const;
         void delete_file_permanently() const;
         void move_file_to_trash() const;
         void request_satisfied() const;
public slots:
         void download_progress_update(std::int64_t received_byte_cnt,std::int64_t total_byte_cnt) noexcept;
         void upload_progress_update(std::int64_t send_byte_cnt,std::int64_t total_byte_cnt) noexcept;
private:
         explicit Download_tracker(const QString & path,QWidget * parent = nullptr);
         
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

         QHBoxLayout download_path_layout_;
         QLabel download_path_buddy_ {"Path:"};
         QLabel download_path_label_;

         QStackedWidget state_holder_;
         QLineEdit error_line_;
         QProgressBar download_progress_bar_;

         QHBoxLayout download_quantity_layout_;
         QLabel download_quantity_buddy_ {"Downloaded:"};
         QLabel download_quantity_label_ {"0 byte (s) / 0 byte (s)"};

         QHBoxLayout upload_quantity_layout_;
         QLabel upload_quantity_buddy_ {"Uploaded:"};
         QLabel upload_quantity_label_ {"0 byte (s) / 0 byte (s)"};

         QStackedWidget terminate_buttons_holder_;
         QPushButton finish_button_ {"Finish"};
         QPushButton cancel_button_ {"Cancel"};

         QStackedWidget initiate_buttons_holder_;
         QPushButton open_button_ {"Open"};
         QPushButton retry_button_ {"Retry"};

         QHBoxLayout time_elapsed_layout_;
         QTime time_elapsed_ {0,0,1}; // 1 to prevent division by zero
         QTimer time_elapsed_timer_;
         QLabel time_elapsed_buddy_ {"Time elapsed:"};
         QLabel time_elapsed_label_ {time_elapsed_.toString() + " hh::mm::ss"};

         QHBoxLayout download_speed_layout_;
         QLabel download_speed_buddy_ {"Download Speed:"};
         QLabel download_speed_label_ {"0 bytes/sec"};

         QPushButton delete_button_ {"Delete"};
         QPushButton open_directory_button_ {"Open directory"};
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

[[nodiscard]]
inline std::uint32_t Download_tracker::get_elapsed_seconds() const noexcept {
         return static_cast<std::uint32_t>(time_elapsed_.second() + time_elapsed_.minute() * 60 + time_elapsed_.hour() * 3600);
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
         upload_quantity_label_.setText(util::conversion::stringify_bytes(send_byte_cnt,total_byte_cnt));
}