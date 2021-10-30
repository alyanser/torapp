#pragma once

#include "util.hxx"

#include <QStackedWidget>
#include <QProgressBar>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QSettings>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QTime>

namespace bencode {
         struct Metadata;
}

class Download_tracker : public QFrame {
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

         enum class State {
                  Download,
                  Verification
         };

         enum class Download_Type {
                  Url,
                  Torrent
         };

         Download_tracker(const QString & dl_path,QUrl url,QWidget * parent = nullptr);
         Download_tracker(const QString & dl_path,bencode::Metadata torrent_metadata,QWidget * parent = nullptr);

         Error error() const noexcept;
         void set_restored_byte_count(std::int64_t restored_byte_cnt) noexcept;

         void set_state(State state) noexcept;
         void set_ratio(double ratio) noexcept;
         void set_error_and_finish(Error error) noexcept;
         void set_status_and_finish(const QString & status) noexcept;
         void download_progress_update(std::int64_t received_byte_cnt,std::int64_t total_byte_cnt = -1) noexcept;
         void verification_progress_update(std::int32_t verified_asset_cnt,std::int32_t total_asset_cnt) noexcept;
         void set_upload_byte_count(std::int64_t uled_byte_cnt) noexcept;
         void switch_to_finished_state() noexcept;
signals:
         void retry_download(const QString & file_path,const QUrl & url) const;
         void retry_download(const QString & file_path,const bencode::Metadata & torrent_metadata) const;
         void delete_file_permanently() const;
         void move_file_to_trash() const;
         void request_satisfied() const;
         void download_dropped() const;
         void download_paused() const;
         void download_resumed() const;
         void properties_button_clicked() const;
public slots:
         void enable_completion_relevant_buttons() noexcept;
private:
         Download_tracker(const QString & dl_path,Download_Type dl_type,QWidget * parent = nullptr);

         void setup_layout() noexcept;
         void configure_default_connections() noexcept;
         void setup_file_status_layout() noexcept;
         void setup_network_status_layout() noexcept;
         void setup_state_stack() noexcept;
         void update_error_line() noexcept;
         void update_download_speed() noexcept;
         void write_settings() const noexcept;
         void read_settings() noexcept;
         void begin_setting_groups(QSettings & settings) const noexcept;
         ///
         constexpr static std::string_view time_elapsed_fmt{" hh:mm:ss"};
         QString dl_path_;
         QVBoxLayout central_layout_{this};
         QHBoxLayout file_stat_layout_;
         QHBoxLayout network_stat_layout_;
         QHBoxLayout package_name_layout_;
         QHBoxLayout dl_quanitity_layout_;
         QHBoxLayout ul_quantity_layout_;
         QHBoxLayout time_elapsed_layout_;
         QHBoxLayout dl_speed_layout_;
         QFormLayout network_form_layout_;
         QStackedWidget state_stack_;
         QStackedWidget terminate_button_stack_;
         QStackedWidget initiate_button_stack_;
         QStackedWidget state_button_stack_;
         QTime time_elapsed_{0,0,0};
         QLineEdit finish_line_;
         QLabel package_name_buddy_{"Name"};
         QLabel package_name_label_;
         QLabel dl_quantity_label_{"0 byte (s) / 0 byte (s)"};
         QLabel ul_quantity_label_{"0 byte (s)"};
         QLabel time_elapsed_buddy_{"Time elapsed"};
         QLabel time_elapsed_label_{time_elapsed_.toString() + time_elapsed_fmt.data()};
         QLabel dl_speed_label_{"0 byte (s) / sec"};
         QLabel ratio_label_{"0"};
         QPushButton finish_button_{"Finish"};
         QPushButton cancel_button_{"Cancel"};
         QPushButton open_button_{"Open"};
         QPushButton retry_button_{"Retry"};
         QPushButton delete_button_{"Delete"};
         QPushButton open_dir_button_{"Open in directory"};
         QPushButton pause_button_{"Pause"};
         QPushButton resume_button_{"Resume"};
         QPushButton properties_button_{"Properties"};
         QProgressBar dl_progress_bar_;
         QProgressBar verify_progress_bar_;
         QTimer session_timer_;
         std::int64_t dled_byte_cnt_ = 0;
         std::int64_t total_byte_cnt_ = 0;
         std::int64_t restored_byte_cnt_ = 0;
         std::chrono::seconds session_time_{};
         State state_ = State::Download;
         Error error_ = Error::Null;
         Download_Type dl_type_;
};

inline void Download_tracker::set_error_and_finish(const Error error) noexcept {
         assert(error != Error::Null && error != Error::Custom);
         error_ = error;
         update_error_line();
         switch_to_finished_state();
}

inline void Download_tracker::set_status_and_finish(const QString & status) noexcept {
         error_ = Error::Custom;
         finish_line_.setText(status);
         switch_to_finished_state();
}

inline void Download_tracker::set_upload_byte_count(const std::int64_t uled_byte_cnt) noexcept {
         const auto [converted_ul_byte_cnt,converted_ul_postfix] = util::conversion::stringify_bytes(uled_byte_cnt,util::conversion::Format::Memory);
         ul_quantity_label_.setText(QString("%1 %2").arg(converted_ul_byte_cnt).arg(converted_ul_postfix.data()));
}

inline Download_tracker::Error Download_tracker::error() const noexcept {
         return error_;
}

inline void Download_tracker::set_restored_byte_count(const std::int64_t restored_byte_cnt) noexcept {
         restored_byte_cnt_ = restored_byte_cnt;
}

inline void Download_tracker::set_ratio(const double ratio) noexcept {
         ratio_label_.setText(QString::number(ratio));
}

inline void Download_tracker::setup_layout() noexcept {
         central_layout_.addLayout(&file_stat_layout_);
         central_layout_.addWidget(&state_stack_);
         central_layout_.addLayout(&network_stat_layout_);
}

inline void Download_tracker::begin_setting_groups(QSettings & settings) const noexcept {
         settings.beginGroup(dl_type_ == Download_Type::Torrent ? "torrent_downloads" : "url_downloads");
         settings.beginGroup(QString(dl_path_).replace('/','\x20'));
}