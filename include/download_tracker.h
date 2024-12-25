#pragma once

#include "util.h"

#include <bencode_parser.h>
#include <QStackedWidget>
#include <QProgressBar>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QTime>

class Download_tracker : public QFrame {
	Q_OBJECT
public:
	enum class Error {
		Null,
		File_Write,
		File_Lock,
		Network,
		Space,
		Invalid_Request,
		Custom
	};

	enum class State {
		Download,
		Verification
	};

	Download_tracker(const QString & dl_path, QUrl url, QWidget * parent = nullptr);
	Download_tracker(const QString & dl_path, bencode::Metadata torrent_metadata, QWidget * parent = nullptr);

	void set_restored_byte_count(const std::int64_t restored_byte_cnt) noexcept {
		restored_byte_cnt_ = restored_byte_cnt;
	}

	void set_error_and_finish(const Error error) noexcept {
		update_finish_line(error);
		switch_to_finished_state(error);
	}

	void set_error_and_finish(const QString & error_desc) noexcept {
		assert(!error_desc.isEmpty());
		finish_line_.setText(error_desc);
		switch_to_finished_state(Error::Custom);
	}

	void set_ratio(const double ratio) noexcept {
		assert(dl_type_ == Download_Type::Torrent);
		assert(ratio >= 0);
		ratio_label_.setText(QString::number(ratio, 'f', 2));
	}

	void set_state(State state) noexcept;
	void download_progress_update(std::int64_t received_byte_cnt, std::int64_t total_byte_cnt = -1) noexcept;
	void verification_progress_update(std::int32_t verified_asset_cnt, std::int32_t total_asset_cnt) noexcept;
	void set_upload_byte_count(std::int64_t uled_byte_cnt) noexcept;
	void on_verification_completed() noexcept;
signals:
	void retry_download(const QString & file_path, QUrl url, QByteArray info_sha1_hash = "") const;
	void retry_download(const QString & file_path, bencode::Metadata torrent_metadata, QByteArray info_sha1_hash = "") const;
	void restored_download_paused() const;
	void torrent_open_button_clicked() const;
	void delete_files_permanently() const;
	void move_files_to_trash() const;
	void request_satisfied() const;
	void download_dropped() const;
	void download_paused() const;
	void download_resumed() const;
	void properties_button_clicked() const;
	void url_download_finished() const;

private:
	enum class Download_Type {
		Url,
		Torrent
	};

	Download_tracker(const QString & dl_path, Download_Type dl_type, QWidget * parent = nullptr);

	void setup_central_layout() noexcept;
	void configure_default_connections() noexcept;
	void setup_file_status_layout() noexcept;
	void setup_network_status_layout() noexcept;
	void setup_state_stack() noexcept;
	void update_finish_line(Error error) noexcept;
	void update_download_speed() noexcept;
	void write_settings() const noexcept;
	void read_settings() noexcept;
	void begin_setting_groups(QSettings & settings) const noexcept;
	void switch_to_finished_state(Error error) noexcept;
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
	QStackedWidget progress_bar_stack_;
	QStackedWidget terminate_button_stack_;
	QStackedWidget initiate_button_stack_;
	QStackedWidget state_button_stack_;
	QTime time_elapsed_{0, 0, 0};
	QLineEdit finish_line_;
	QLabel package_name_buddy_{"Name"};
	QLabel package_name_label_;
	QLabel dl_quantity_label_{"0 byte (s) / 0 byte (s)"};
	QLabel ul_quantity_label_{"0 byte (s)"};
	QLabel time_elapsed_buddy_{"Time elapsed"};
	QLabel time_elapsed_label_{time_elapsed_.toString() + time_elapsed_fmt.data()};
	QLabel dl_speed_label_{"0 byte (s) / sec"};
	QLabel ratio_label_{"0.00"};
	QPushButton finish_button_{"Finish"};
	QPushButton cancel_button_{"Cancel"};
	QPushButton open_button_{"Open"};
	QPushButton retry_button_{"Retry"};
	QPushButton delete_button_{"Delete"};
	QPushButton open_dir_button_{"Open directory"};
	QPushButton pause_button_{"Pause"};
	QPushButton resume_button_{"Resume"};
	QPushButton properties_button_{"Properties"};
	QProgressBar dl_progress_bar_;
	QProgressBar verify_progress_bar_;
	QTimer session_timer_;
	std::int64_t dled_byte_cnt_ = 0;
	std::int64_t session_dled_byte_cnt_ = 0;
	std::int64_t total_byte_cnt_ = 0;
	std::int64_t restored_byte_cnt_ = 0;
	std::chrono::seconds session_time_{};
	State state_ = State::Download;
	Download_Type dl_type_;
	bool dl_paused_ = false;
	bool restored_dl_paused_ = false;
};