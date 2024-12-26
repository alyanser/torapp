#include "download_tracker.h"

#include <bencode_parser.h>
#include <QDesktopServices>
#include <QMessageBox>
#include <QSettings>
#include <QDir>

Download_tracker::Download_tracker(const QString & dl_path, const Download_Type dl_type, QWidget * const parent) : QFrame(parent), dl_path_(dl_path), dl_type_(dl_type) {
	setFixedHeight(205);
	setLineWidth(3);

	setFrameShadow(QFrame::Shadow::Sunken);
	setFrameShape(QFrame::Shape::Box);

	setup_central_layout();
	setup_file_status_layout();
	setup_network_status_layout();
	setup_state_stack();
	configure_default_connections();

	session_timer_.setInterval(std::chrono::seconds(1));

	pause_button_.setEnabled(false);
	resume_button_.setEnabled(false);
	delete_button_.setEnabled(false);
	open_button_.setEnabled(false);

	package_name_label_.setFrameShadow(QFrame::Shadow::Raised);
	package_name_label_.setFrameShape(QFrame::Shape::Panel);
	package_name_label_.setAlignment(Qt::AlignCenter);

	time_elapsed_label_.setFrameShadow(QFrame::Shadow::Raised);
	time_elapsed_label_.setFrameShape(QFrame::Shape::Panel);
	time_elapsed_label_.setAlignment(Qt::AlignCenter);

	dl_progress_bar_.setTextVisible(true);
	dl_progress_bar_.setValue(0);
	dl_progress_bar_.setRange(0, 0);

	verify_progress_bar_.setTextVisible(true);

	finish_line_.setAlignment(Qt::AlignCenter);

	read_settings();

	auto open_url = [this](const QString & path) {
		if(!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
			QMessageBox::critical(this, "Open error", "Given path could not be opened");
		}
	};

	connect(&open_dir_button_, &QPushButton::clicked, this, [dl_path, open_url] {
		QFileInfo file_info(dl_path);
		open_url(file_info.isDir() ? file_info.absoluteFilePath() : file_info.absolutePath());
	});

	connect(&open_button_, &QPushButton::clicked, this, [this, dl_type_ = dl_type_, dl_path, open_url] {
		if(dl_type_ == Download_Type::Torrent) {
			emit torrent_open_button_clicked();
		} else {
			QFileInfo file_info(dl_path);
			open_url(file_info.absoluteFilePath());
		}
	});
}

Download_tracker::Download_tracker(const QString & dl_path, QUrl url, QWidget * const parent) : Download_tracker(dl_path, Download_Type::Url, parent) {
	assert(!url.isEmpty());
	assert(!dl_path.isEmpty());

	package_name_label_.setText(QUrl(dl_path).fileName());

	auto restart_download = [this, dl_path, url = std::move(url)]() mutable {
		emit retry_download(dl_path, std::move(url));
		emit request_satisfied();
	};

	connect(&retry_button_, &QPushButton::clicked, this, restart_download, Qt::SingleShotConnection);
	connect(&resume_button_, &QPushButton::clicked, this, restart_download, Qt::SingleShotConnection);
}

Download_tracker::Download_tracker(const QString & dl_path, bencode::Metadata torrent_metadata, QWidget * const parent) : Download_tracker(dl_path, Download_Type::Torrent, parent) {
	package_name_label_.setText(torrent_metadata.name.empty() ? "N/A" : torrent_metadata.name.data());

	connect(
	    &retry_button_, &QPushButton::clicked, this,
	    [this, dl_path, torrent_metadata = std::move(torrent_metadata)]() mutable {
		    emit retry_download(dl_path, std::move(torrent_metadata));
		    emit request_satisfied();
	    },
	    Qt::SingleShotConnection);
}

void Download_tracker::setup_central_layout() noexcept {
	central_layout_.setSpacing(15);
	central_layout_.addLayout(&file_stat_layout_);
	central_layout_.addWidget(&progress_bar_stack_);
	central_layout_.addLayout(&network_stat_layout_);
}

void Download_tracker::setup_file_status_layout() noexcept {
	file_stat_layout_.addLayout(&package_name_layout_);
	file_stat_layout_.addLayout(&time_elapsed_layout_);
	file_stat_layout_.addWidget(&open_dir_button_);

	if(dl_type_ == Download_Type::Torrent) {
		file_stat_layout_.addWidget(&properties_button_);
	}

	package_name_layout_.addWidget(&package_name_buddy_);
	package_name_layout_.addWidget(&package_name_label_);
	package_name_buddy_.setBuddy(&package_name_label_);

	time_elapsed_layout_.addWidget(&time_elapsed_buddy_);
	time_elapsed_layout_.addWidget(&time_elapsed_label_);
	time_elapsed_buddy_.setBuddy(&time_elapsed_label_);
}

void Download_tracker::setup_network_status_layout() noexcept {
	network_form_layout_.setFormAlignment(Qt::AlignCenter);
	network_form_layout_.setSpacing(10);

	network_stat_layout_.addLayout(&network_form_layout_);

	network_form_layout_.addRow("Download Speed", &dl_speed_label_);
	network_form_layout_.addRow("Downloaded", &dl_quantity_label_);

	network_stat_layout_.addWidget(&delete_button_);

	if(dl_type_ == Download_Type::Torrent) { // todo: also support download interruption for url type
		// ? make it dynamic
		network_form_layout_.addRow("Uploaded", &ul_quantity_label_);
		network_form_layout_.addRow("Session ratio", &ratio_label_);
		network_stat_layout_.addWidget(&state_button_stack_);
		state_button_stack_.addWidget(&pause_button_);
		state_button_stack_.addWidget(&resume_button_);
	}

	network_stat_layout_.addWidget(&initiate_button_stack_);
	network_stat_layout_.addWidget(&terminate_button_stack_);

	initiate_button_stack_.addWidget(&open_button_);
	initiate_button_stack_.addWidget(&retry_button_);

	terminate_button_stack_.addWidget(&cancel_button_);
	terminate_button_stack_.addWidget(&finish_button_);
}

void Download_tracker::set_upload_byte_count(const std::int64_t uled_byte_cnt) noexcept {
	assert(dl_type_ == Download_Type::Torrent);
	const auto [converted_ul_byte_cnt, ul_byte_postfix] = util::conversion::stringify_bytes(uled_byte_cnt, util::conversion::Format::Memory);
	ul_quantity_label_.setText(QString::number(converted_ul_byte_cnt, 'f', 2) + ' ' + ul_byte_postfix.data());
}

void Download_tracker::begin_setting_groups(QSettings & settings) const noexcept {
	settings.beginGroup(dl_type_ == Download_Type::Torrent ? "torrent_downloads" : "url_downloads");
	settings.beginGroup(QString(dl_path_).replace('/', '\x20'));
}

void Download_tracker::on_verification_completed() noexcept {
	assert(dl_type_ == Download_Type::Torrent);
	assert(dled_byte_cnt_ >= 0 && dled_byte_cnt_ <= total_byte_cnt_);
	pause_button_.setEnabled(!total_byte_cnt_ || dled_byte_cnt_ != total_byte_cnt_);
}

void Download_tracker::download_progress_update(const std::int64_t received_byte_cnt, const std::int64_t total_byte_cnt) noexcept {

	if(!total_byte_cnt) {
		return;
	}

	assert(received_byte_cnt >= 0);

	{
		const auto newly_dled_byte_cnt = received_byte_cnt - dled_byte_cnt_;
		assert(newly_dled_byte_cnt >= 0);

		dled_byte_cnt_ = received_byte_cnt;
		total_byte_cnt_ = total_byte_cnt;

		state_ == State::Verification ? restored_byte_cnt_ = dled_byte_cnt_ : session_dled_byte_cnt_ += newly_dled_byte_cnt;
	}

	if(constexpr auto unknown_byte_cnt = -1; total_byte_cnt == unknown_byte_cnt) {
		dl_progress_bar_.setRange(0, 0); // sets in pending state
	} else {
		constexpr auto progress_bar_max = 100;
		dl_progress_bar_.setRange(0, progress_bar_max);
		assert(received_byte_cnt <= total_byte_cnt);
		const auto progress_bar_val = static_cast<double>(received_byte_cnt) / static_cast<double>(total_byte_cnt) * progress_bar_max;
		assert(progress_bar_val >= 0 && progress_bar_val <= 100);
		dl_progress_bar_.setValue(static_cast<std::int32_t>(progress_bar_val));
	}

	const auto text_fmt = util::conversion::stringify_bytes(received_byte_cnt, total_byte_cnt);
	dl_quantity_label_.setText(text_fmt);
	dl_progress_bar_.setFormat(text_fmt + (total_byte_cnt < 1 ? " nan %" : ' ' + util::conversion::convert_to_percent_format(received_byte_cnt, total_byte_cnt)));
}

void Download_tracker::set_state(const State state) noexcept {
	state_ = state;

	if(state_ == State::Download) {
		restored_dl_paused_ ? state_button_stack_.setCurrentWidget(&resume_button_) : session_timer_.start();

		pause_button_.setEnabled(true);
		resume_button_.setEnabled(true);

		progress_bar_stack_.setCurrentWidget(&dl_progress_bar_);
	} else {
		assert(dl_type_ == Download_Type::Torrent);
		assert(state_ == State::Verification);
		progress_bar_stack_.setCurrentWidget(&verify_progress_bar_);
	}
}

void Download_tracker::update_download_speed() noexcept {
	assert(session_time_.count() >= 0);
	assert(session_dled_byte_cnt_ >= 0);
	const auto speed = session_time_.count() ? session_dled_byte_cnt_ / session_time_.count() : 0;
	const auto [converted_speed, speed_postfix] = stringify_bytes(speed, util::conversion::Format::Speed);
	dl_speed_label_.setText(QString::number(converted_speed, 'f', 0) + ' ' + speed_postfix.data());
}

void Download_tracker::setup_state_stack() noexcept {
	progress_bar_stack_.addWidget(&dl_progress_bar_);
	progress_bar_stack_.addWidget(&verify_progress_bar_);
	progress_bar_stack_.addWidget(&finish_line_);
	assert(progress_bar_stack_.currentWidget() == &dl_progress_bar_);
}

void Download_tracker::verification_progress_update(const std::int32_t verified_asset_cnt, const std::int32_t total_asset_cnt) noexcept {
	assert(total_asset_cnt);
	assert(state_ == State::Verification);
	verify_progress_bar_.setValue(verified_asset_cnt);
	verify_progress_bar_.setMaximum(total_asset_cnt);
	verify_progress_bar_.setFormat("Verifying " + util::conversion::convert_to_percent_format(verified_asset_cnt, total_asset_cnt));
}

void Download_tracker::write_settings() const noexcept {
	QSettings settings;
	begin_setting_groups(settings);
	settings.setValue("time_elapsed", QVariant::fromValue(time_elapsed_));
	settings.setValue("download_paused", dl_paused_);
}

void Download_tracker::read_settings() noexcept {
	QSettings settings;
	begin_setting_groups(settings);
	restored_dl_paused_ = dl_type_ == Download_Type::Torrent && qvariant_cast<bool>(settings.value("download_paused", false));
	time_elapsed_ = qvariant_cast<QTime>(settings.value("time_elapsed", QTime(0, 0, 0)));
	time_elapsed_label_.setText(time_elapsed_.toString() + time_elapsed_fmt.data());
}

void Download_tracker::configure_default_connections() noexcept {
	connect(this, &Download_tracker::request_satisfied, &Download_tracker::deleteLater);
	connect(this, &Download_tracker::download_dropped, this, &Download_tracker::request_satisfied);
	connect(this, &Download_tracker::delete_files_permanently, this, &Download_tracker::download_dropped);
	connect(this, &Download_tracker::move_files_to_trash, this, &Download_tracker::download_dropped);
	connect(&finish_button_, &QPushButton::clicked, this, &Download_tracker::download_dropped);

	if(dl_type_ == Download_Type::Torrent) {
		connect(&properties_button_, &QPushButton::clicked, this, &Download_tracker::properties_button_clicked);
	}

	connect(&pause_button_, &QPushButton::clicked, this, [this] {
		assert(dl_type_ == Download_Type::Torrent);

		assert(state_button_stack_.currentWidget() == &pause_button_);
		state_button_stack_.setCurrentWidget(&resume_button_);

		assert(session_timer_.isActive());
		session_timer_.stop();

		session_dled_byte_cnt_ = 0;
		session_time_ = std::chrono::seconds::zero();
		dl_paused_ = true;

		update_download_speed();
		emit download_paused();
	});

	connect(&resume_button_, &QPushButton::clicked, this, [this] {
		assert(dl_type_ == Download_Type::Torrent);

		assert(!session_timer_.isActive());
		session_timer_.start();

		assert(state_button_stack_.currentWidget() == &resume_button_);
		state_button_stack_.setCurrentWidget(&pause_button_);
		dl_paused_ = false;

		emit download_resumed();
	});

	connect(&delete_button_, &QPushButton::clicked, this, [this] {
		QMessageBox query_box(QMessageBox::Icon::Warning, "Delete file (s)", "Delete?", QMessageBox::Button::NoButton);

		auto * const delete_permanently_button = query_box.addButton("Delete permanently", QMessageBox::ButtonRole::DestructiveRole);
		auto * const move_to_trash_button = query_box.addButton("Move to Trash", QMessageBox::ButtonRole::YesRole);
		query_box.addButton("Cancel", QMessageBox::ButtonRole::RejectRole);

		connect(delete_permanently_button, &QPushButton::clicked, this, &Download_tracker::delete_files_permanently);
		connect(move_to_trash_button, &QPushButton::clicked, this, &Download_tracker::move_files_to_trash);

		query_box.exec();
	});

	connect(&cancel_button_, &QPushButton::clicked, this, [this] {
		constexpr auto buttons = QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No;
		const auto reply = QMessageBox::question(this, "Cancel download", "Are you sure you want to cancel the download?", buttons);

		if(reply == QMessageBox::StandardButton::Yes) {
			session_timer_.stop();
			emit download_dropped();
		}
	});

	session_timer_.callOnTimeout(this, [this] {
		time_elapsed_ = time_elapsed_.addSecs(1);
		time_elapsed_label_.setText(time_elapsed_.toString() + time_elapsed_fmt.data());
		++session_time_;
		update_download_speed();
		write_settings();
	});
}

void Download_tracker::switch_to_finished_state(const Error error) noexcept {
	time_elapsed_buddy_.setText("Time took: ");
	progress_bar_stack_.setCurrentWidget(&finish_line_);
	terminate_button_stack_.setCurrentWidget(&finish_button_);
	pause_button_.setEnabled(false);
	resume_button_.setEnabled(false);
	session_timer_.stop();
	dl_speed_label_.setText("0 byte (s) / sec");

	if(error == Error::Null) {
		finish_line_.setStyleSheet("background-color:lightgreen");
		assert(initiate_button_stack_.currentWidget() == &open_button_);
		assert(!delete_button_.isEnabled());
		assert(!open_button_.isEnabled());
		delete_button_.setEnabled(true);
		open_button_.setEnabled(true);
	} else {
		finish_line_.setStyleSheet("background-color:tomato");
		initiate_button_stack_.setCurrentWidget(&retry_button_);
	}

	if(dl_type_ == Download_Type::Url) {
		emit url_download_finished();
	}
}

void Download_tracker::update_finish_line(const Error error) noexcept {
	assert(error != Error::Custom);

	finish_line_.setText([error, dl_type_ = dl_type_]() -> QString {
		switch(error) {

			case Error::Null: {
				return dl_type_ == Download_Type::Torrent ? "Seeding" : "Download finished";
			}

			case Error::File_Write: {
				return "Given path could not be opened for writing";
			}

			case Error::Network: {
				return "Unknown network error";
			}

			case Error::File_Lock: {
				return "Given path is locked by another process";
			}

			case Error::Space: {
				return "Not enough space";
			}

			case Error::Invalid_Request: {
				return QString("Invalid request. Possibly corrupted path or invalid ") + (dl_type_ == Download_Type::Torrent ? "torrent file" : "url");
			}

			default: {
				return "Oops. Something went wrong :(";
			}
		}
	}());
}