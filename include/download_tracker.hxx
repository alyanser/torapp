#pragma once

#include "util.hxx"

#include <QStackedWidget>
#include <QFormLayout>
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

         enum class State {
                  Download,
                  Verification
         };

         Download_tracker(const QString & dl_path,QUrl url,QWidget * parent = nullptr);
         Download_tracker(const QString & dl_path,bencode::Metadata torrent_metadata,QWidget * parent = nullptr);

         constexpr Error error() const noexcept;
         constexpr void set_downloaded_bytes_offset(std::int64_t dl_byte_offset) noexcept;

         void set_state(State state) noexcept;
         std::int32_t get_elapsed_seconds() const noexcept;
         void set_error_and_finish(Error error) noexcept;
         void set_error_and_finish(const QString & custom_error) noexcept;
         void download_progress_update(std::int64_t received_byte_cnt,std::int64_t total_byte_cnt = -1) noexcept;
         void verification_progress_update(std::int32_t verified_asset_cnt,std::int32_t total_asset_cnt) noexcept;
         void upload_progress_update(std::int64_t send_byte_cnt,std::int64_t total_byte_cnt) noexcept;
         void switch_to_finished_state() noexcept;
signals:
         void retry_download(const QString & file_path,const QUrl & url) const;
         void retry_download(const QString & file_path,const bencode::Metadata & torrent_metadata) const;
         void delete_file_permanently() const;
         void move_file_to_trash() const;
         void request_satisfied() const;
         void download_dropped() const;
private:
         explicit Download_tracker(const QString & dl_path,QWidget * parent = nullptr);

         void setup_layout() noexcept;
         void configure_default_connections() noexcept;
         void setup_file_status_layout() noexcept;
         void setup_network_status_layout() noexcept;
         void setup_state_holder() noexcept;
         void update_error_line() noexcept;
         void update_download_speed() noexcept;
         ///
         QVBoxLayout central_layout_{this};
         QHBoxLayout file_stat_layout_;
         QHBoxLayout network_stat_layout_;
         QHBoxLayout package_name_layout_;
         QHBoxLayout dl_path_layout_;
         QHBoxLayout dl_quanitity_layout_;
         QHBoxLayout ul_quantity_layout_;
         QHBoxLayout time_elapsed_layout_;
         QHBoxLayout dl_speed_layout_;
         QFormLayout network_form_layout_;
         QStackedWidget state_holder_;
         QStackedWidget terminate_buttons_holder_;
         QStackedWidget initiate_buttons_holder_;
         QTime time_elapsed_{0,0,0};
         QLineEdit finish_line_;
         QLabel package_name_buddy_{"Name:"};
         QLabel package_name_label_;
         QLabel dl_path_buddy_{"Path:"};
         QLabel dl_path_label;
         QLabel dl_quantity_label_{"0 byte (s) / 0 byte (s)"};
         QLabel ul_quantity_label_{"0 byte (s) / 0 byte (s)"};
         QLabel time_elapsed_buddy_{"Time elapsed:"};
         QLabel time_elapsed_label_{time_elapsed_.toString() + " hh::mm::ss"};
         QLabel dl_speed_label_{"0 byte (s) / sec"};
         QPushButton finish_button_{"Finish"};
         QPushButton cancel_button_{"Cancel"};
         QPushButton open_button_{"Open"};
         QPushButton retry_button_{"Retry"};
         QPushButton delete_button_{"Delete"};
         QPushButton open_dir_button_{"Open in directory"};
         QProgressBar dl_progress_bar_;
         QProgressBar verify_progress_bar_;
         QTimer refresh_timer_;
         std::int64_t dl_byte_offset_ = 0;
         std::int64_t dled_byte_cnt_ = 0;
         std::int64_t uled_byte_cnt_ = 0;
         std::int64_t total_byte_cnt_ = 0;
         std::int64_t restored_byte_cnt_ = 0;
         State state_{State::Download};
         Error error_{Error::Null};
};

[[nodiscard]]
inline std::int32_t Download_tracker::get_elapsed_seconds() const noexcept {
         assert(time_elapsed_.second() + time_elapsed_.minute() * 60 + time_elapsed_.hour() * 3600 > 0);
         return 1 + time_elapsed_.second() + time_elapsed_.minute() * 60 + time_elapsed_.hour() * 3600;
}

inline void Download_tracker::set_error_and_finish(const Error error) noexcept {
         assert(error != Error::Null && error != Error::Custom);
         error_ = error;
         update_error_line();
         switch_to_finished_state();
}

inline void Download_tracker::set_error_and_finish(const QString & custom_error) noexcept {
         error_ = Error::Custom;
         finish_line_.setText(custom_error);
         switch_to_finished_state();
}

inline void Download_tracker::upload_progress_update(const std::int64_t send_byte_cnt,const std::int64_t total_byte_cnt) noexcept {
         ul_quantity_label_.setText(util::conversion::stringify_bytes(send_byte_cnt,total_byte_cnt));
}

[[nodiscard]]
constexpr Download_tracker::Error Download_tracker::error() const noexcept {
         return error_;
}

constexpr void Download_tracker::set_downloaded_bytes_offset(const std::int64_t dl_byte_offset) noexcept {
         dl_byte_offset_ = dl_byte_offset;
}

inline void Download_tracker::set_state(const State state) noexcept {
         state_ = state;

         if(state_ == State::Download){
                  refresh_timer_.start(std::chrono::seconds(1));
                  state_holder_.setCurrentWidget(&dl_progress_bar_);
         }else{
                  assert(state_ == State::Verification);
                  state_holder_.setCurrentWidget(&verify_progress_bar_);
         }
}

inline void Download_tracker::setup_layout() noexcept {
         central_layout_.addLayout(&file_stat_layout_);
         central_layout_.addWidget(&state_holder_);
         central_layout_.addLayout(&network_stat_layout_);
}

inline void Download_tracker::update_download_speed() noexcept {
         assert(dled_byte_cnt_ >= restored_byte_cnt_);
         assert(get_elapsed_seconds() > 0);
         const auto speed = (dled_byte_cnt_ - restored_byte_cnt_) / get_elapsed_seconds();
         const auto [converted_speed,speed_postfix] = stringify_bytes(static_cast<double>(speed),util::conversion::Conversion_Format::Speed);
         dl_speed_label_.setText(QString("%1 %2").arg(converted_speed).arg(speed_postfix.data()));
}

inline void Download_tracker::setup_state_holder() noexcept {
         state_holder_.addWidget(&dl_progress_bar_);
         state_holder_.addWidget(&verify_progress_bar_);
         state_holder_.addWidget(&finish_line_);

         dl_progress_bar_.setMinimum(0);
         dl_progress_bar_.setValue(0);
         
         finish_line_.setAlignment(Qt::AlignCenter);
         assert(state_holder_.currentWidget() == &dl_progress_bar_);
}

inline void Download_tracker::verification_progress_update(std::int32_t verified_asset_cnt,std::int32_t total_asset_cnt) noexcept {
         assert(total_asset_cnt);
         assert(state_ == State::Verification);
         verify_progress_bar_.setValue(verified_asset_cnt);
         verify_progress_bar_.setMaximum(total_asset_cnt);
         verify_progress_bar_.setFormat("Verifying " + QString::number(util::conversion::convert_to_percentile(verified_asset_cnt,total_asset_cnt)) + "%");
}