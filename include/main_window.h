#pragma once

#include "network_manager.h"
#include "file_allocator.h"

#include <QSystemTrayIcon>
#include <QScrollArea>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>

class Main_window : public QMainWindow {
	Q_OBJECT
public:
	Main_window();
	Q_DISABLE_COPY_MOVE(Main_window)

	~Main_window() override {
		write_settings();
	}

	template<typename dl_metadata_type>
	void initiate_download(const QString & dl_path, dl_metadata_type dl_metadata, QByteArray info_sha1_hash = "") noexcept;

signals:
	void closed() const;

protected:
	void closeEvent(QCloseEvent * event) noexcept override;

private:
	template<typename dl_metadata_type>
	void add_download_to_settings(const QString & path, dl_metadata_type && dl_metadata) const noexcept;

	template<typename dl_metadata_type>
	void remove_download_from_settings(const QString & file_path) const noexcept;

	template<typename dl_metadata_type>
	void restore_downloads() noexcept;

	void write_settings() const noexcept;
	void add_top_actions() noexcept;
	void read_settings() noexcept;
	void configure_tray_icon() noexcept;
	///
	QSystemTrayIcon tray_{QIcon(":app_icon.png"), this};
	QScrollArea scroll_area_;
	QWidget scroll_area_widget_{&scroll_area_};
	QVBoxLayout central_layout_{&scroll_area_widget_};
	QToolBar tool_bar_;
	QMenu file_menu_{"File", menuBar()};
	Network_manager network_manager_;
	File_allocator file_manager_;
};
