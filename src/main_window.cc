#include "main_window.h"
#include "torrent_metadata_dialog.h"
#include "url_input_dialog.h"
#include "download_tracker.h"
#include "magnet_url_parser.h"
#include "util.h"

#include <bencode_parser.h>
#include <QCloseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QFileInfo>
#include <QTimer>
#include <QFile>

Main_window::Main_window() {
	setWindowTitle("Torapp");
	setCentralWidget(&scroll_area_);
	addToolBar(&tool_bar_);
	setMinimumSize({640, 480});
	assert(menuBar());
	menuBar()->addMenu(&file_menu_);
	add_top_actions();
	configure_tray_icon();
	read_settings();

	central_layout_.setSpacing(20);
	central_layout_.setAlignment(Qt::AlignTop);
	assert(scroll_area_widget_.layout());
	scroll_area_.setWidget(&scroll_area_widget_);
	scroll_area_.setWidgetResizable(true);

	connect(&network_manager_, &Network_manager::new_download_requested, this, &Main_window::initiate_download<bencode::Metadata>);
}

Main_window::~Main_window() { write_settings(); }

void Main_window::configure_tray_icon() noexcept {

	{
		auto * const tray_menu = new QMenu(this);
		auto * const show_action = new QAction("Show Torapp", tray_menu);
		auto * const quit_action = new QAction("Quit Torapp", tray_menu);

		tray_menu->addAction(show_action);
		tray_menu->addAction(quit_action);

		tray_.setContextMenu(tray_menu);

		connect(show_action, &QAction::triggered, this, &Main_window::show); // ! doesn't always work
		connect(quit_action, &QAction::triggered, this, &Main_window::closed);
	}

	tray_.show();
}

void Main_window::closeEvent(QCloseEvent * const event) noexcept {
	const auto reply_button = QMessageBox::question(this, "Quit", "Are you sure you want to quit? All of the downloads (if any) will be stopped.");
	reply_button == QMessageBox::Yes ? event->accept(), emit closed() : event -> ignore();
}

void Main_window::write_settings() const noexcept {
	QSettings settings;
	settings.beginGroup("main_window");
	settings.setValue("size", size());
	settings.setValue("pos", pos());
}

void Main_window::read_settings() noexcept {
	QSettings settings;
	settings.beginGroup("main_window");

	if(settings.contains("size")) {
		resize(settings.value("size").toSize());
		move(settings.value("pos", QPoint(0, 0)).toPoint());
		show();
	} else {
		showMaximized();
	}

	QTimer::singleShot(0, this, [this] {
		restore_downloads<QUrl>();
		restore_downloads<bencode::Metadata>();
	});
}

void Main_window::add_top_actions() noexcept {
	auto * const magnet_action = tool_bar_.addAction("Torrent (magnet)");
	auto * const torrent_action = tool_bar_.addAction("Torrent (file)");
	auto * const url_action = tool_bar_.addAction("Custom url");
	auto * const exit_action = new QAction("Exit", &file_menu_);

	assert(magnet_action->parent() && torrent_action->parent() && url_action->parent() && exit_action->parent());

	file_menu_.addAction(magnet_action);
	file_menu_.addAction(torrent_action);
	file_menu_.addAction(url_action);
	file_menu_.addAction(exit_action);

	magnet_action->setToolTip("Download a torrent using magnet url");
	torrent_action->setToolTip("Download a torrent using .torrent file");
	url_action->setToolTip("Download a file from custom url");
	exit_action->setToolTip("Exit Torapp");

	connect(exit_action, &QAction::triggered, this, &Main_window::closed);

	connect(torrent_action, &QAction::triggered, this, [this] {
		auto file_path = QFileDialog::getOpenFileName(this, "Choose a torrent file", QDir::currentPath(), "Torrent (*.torrent);; All files (*.*)");

		if(file_path.isEmpty()) {
			return;
		}

		Torrent_metadata_dialog torrent_dialog(file_path, this);

		connect(&torrent_dialog, &Torrent_metadata_dialog::new_request_received, this, [this, file_path = std::move(file_path)](const QString & dl_dir) {
			if(QFile torrent_file(file_path); torrent_file.open(QFile::ReadOnly)) {
				add_download_to_settings(dl_dir, torrent_file.readAll());
			}
		});

		connect(&torrent_dialog, &Torrent_metadata_dialog::new_request_received, this, &Main_window::initiate_download<bencode::Metadata>);

		torrent_dialog.exec();
	});

	connect(url_action, &QAction::triggered, this, [this] {
		Url_input_dialog url_dialog(this);

		connect(&url_dialog, &Url_input_dialog::new_request_received, this, &Main_window::initiate_download<QUrl>);

		connect(&url_dialog, &Url_input_dialog::new_request_received, this, [this](const QString & file_path, const QUrl & url) {
			assert(!file_path.isEmpty());
			assert(!url.isEmpty());
			add_download_to_settings(file_path, url);
		});

		url_dialog.exec();
	});

	connect(magnet_action, &QAction::triggered, this, [this] {
		Url_input_dialog magnet_dialog(this);

		connect(&magnet_dialog, &Url_input_dialog::new_request_received, this, [this](const QString & file_path, const QUrl & magnet_url) {
			const auto torrent_metadata = magnet::parse(magnet_url.toString().toLatin1());

			if(!torrent_metadata) {
				QMessageBox::critical(this, "Invalid magnet url", "Given magnet url could not be parsed.");
				tray_.showMessage("Download start failed", "Download could not be started due to invalid magnet url");
				return;
			}

			if(torrent_metadata->tracker_urls.empty()) {
				// todo: implement DHT protocol
				qDebug() << "torrent requires DHT protocol. [not implemented yet]";
				return;
			}

			auto * const tracker = new Download_tracker(file_path, magnet_url, &scroll_area_widget_);

			central_layout_.addWidget(tracker);
			network_manager_.download(file_path, *torrent_metadata, tracker);
			tray_.showMessage("Metadata retrieval started", "Torapp is trying to retrieve torrent metadata from peers. Please wait");
		});

		magnet_dialog.exec();
	});
}

template<typename dl_metadata_type>
void Main_window::initiate_download(const QString & dl_path, dl_metadata_type dl_metadata, QByteArray info_sha1_hash) noexcept {
	static_assert(!std::is_reference_v<dl_metadata_type>);

	auto * const tracker = new Download_tracker(dl_path, dl_metadata, &scroll_area_widget_);
	central_layout_.addWidget(tracker);

	{
		const auto tracker_signal = qOverload<decltype(dl_path), dl_metadata_type, decltype(info_sha1_hash)>(&Download_tracker::retry_download);
		connect(tracker, tracker_signal, this, &Main_window::initiate_download<dl_metadata_type>);
	}

	{
		auto remove_dl = [this, dl_path] { remove_download_from_settings<dl_metadata_type>(dl_path); };

		connect(tracker, &Download_tracker::download_dropped, this, remove_dl);

		if constexpr(std::is_same_v<std::remove_const_t<dl_metadata_type>, QUrl>) {
			connect(tracker, &Download_tracker::url_download_finished, this, remove_dl);
		}
	}

	auto file_handles = file_manager_.open_file_handles(dl_path, dl_metadata);

	if(file_handles.has_value()) {
		assert(!file_handles->isEmpty());

		if constexpr(std::is_same_v<std::remove_const_t<dl_metadata_type>, QUrl>) {
			network_manager_.download({dl_path, std::move(*file_handles), tracker}, std::move(dl_metadata));
		} else {
			network_manager_.download({dl_path, std::move(*file_handles), tracker}, std::move(dl_metadata), std::move(info_sha1_hash));
		}

		tray_.showMessage("Download started", "Download has successfully started");
		return;
	}

	switch(file_handles.error()) {

		case File_allocator::Error::Invalid_Request: {
			tracker->set_error_and_finish(Download_tracker::Error::Invalid_Request);
			tray_.showMessage("Download start failed", "Invalid download request");
			break;
		}

		case File_allocator::Error::File_Lock: {
			tracker->set_error_and_finish(Download_tracker::Error::File_Lock);
			tray_.showMessage("Download start failed", "Download could not be started due to a file lock");
			break;
		}

		case File_allocator::Error::Permissions: {
			tracker->set_error_and_finish(Download_tracker::Error::File_Write);
			tray_.showMessage("Download start failed", "You do not have enough permissions to save files in the given path");
			break;
		}
	}
}

template<typename dl_metadata_type>
void Main_window::add_download_to_settings(const QString & path, dl_metadata_type && dl_metadata) const noexcept {
	QSettings settings;
	util::begin_setting_group<dl_metadata_type>(settings);
	settings.beginGroup(QString(path).replace('/', '\x20'));
	settings.setValue("path", path);
	settings.setValue("download_metadata", std::forward<dl_metadata_type>(dl_metadata));
}

template<typename dl_metadata_type>
void Main_window::remove_download_from_settings(const QString & file_path) const noexcept {
	QSettings settings;
	util::begin_setting_group<dl_metadata_type>(settings);
	settings.beginGroup(QString(file_path).replace('/', '\x20'));
	settings.remove("");
	assert(settings.childKeys().isEmpty());
}

template<typename dl_metadata_type>
void Main_window::restore_downloads() noexcept {
	QSettings settings;
	util::begin_setting_group<dl_metadata_type>(settings);

	const auto child_groups = settings.childGroups();

	std::ranges::for_each(std::as_const(child_groups), [this, &settings](const auto & dl_group) {
		settings.beginGroup(dl_group);

		constexpr auto is_url_download = std::is_same_v<std::remove_cv_t<std::remove_reference_t<dl_metadata_type>>, QUrl>;

		const auto dl_metadata = [&settings] {
			if constexpr(is_url_download) {
				return qvariant_cast<QUrl>(settings.value("download_metadata"));
			} else {
				return qvariant_cast<QByteArray>(settings.value("download_metadata"));
			}
		}();

		auto path = qvariant_cast<QString>(settings.value("path"));

		if(dl_metadata.isEmpty() || path.isEmpty()) {
			return;
		}

		QTimer::singleShot(0, this, [this, path = std::move(path), dl_metadata = std::move(dl_metadata)]() mutable {
			if constexpr(is_url_download) {
				dl_metadata.isValid() ? initiate_download(path, std::move(dl_metadata)) : remove_download_from_settings<QUrl>(path);
			} else {
				const auto torrent_metadata = [dl_metadata = std::move(dl_metadata)]() mutable -> std::optional<bencode::Metadata> {
					const auto compl_file_content = dl_metadata.toStdString();

					try {
						return bencode::extract_metadata(bencode::parse_content(compl_file_content), compl_file_content);
					} catch(const std::exception & exception) {
						qDebug() << exception.what();
						return {};
					}
				}();

				torrent_metadata ? initiate_download(path, std::move(*torrent_metadata)) : remove_download_from_settings<bencode::Metadata>(path);
			}
		});

		settings.endGroup();
	});
}

template void Main_window::initiate_download<bencode::Metadata>(const QString &, bencode::Metadata, QByteArray) noexcept;
template void Main_window::initiate_download<QUrl>(const QString &, QUrl, QByteArray) noexcept;
template void Main_window::add_download_to_settings<QUrl>(const QString &, QUrl &&) const noexcept;
template void Main_window::add_download_to_settings<QString>(const QString &, QString &&) const noexcept;
template void Main_window::remove_download_from_settings<bencode::Metadata>(const QString &) const noexcept;
template void Main_window::remove_download_from_settings<QUrl>(const QString &) const noexcept;
template void Main_window::restore_downloads<QUrl>() noexcept;
template void Main_window::restore_downloads<bencode::Metadata>() noexcept;
