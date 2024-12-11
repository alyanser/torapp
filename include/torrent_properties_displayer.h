#pragma once

#include <QScrollArea>
#include <QTableWidget>
#include <QFormLayout>
#include <QTabWidget>

namespace bencode {

struct Metadata;

}

namespace magnet {

struct Metadata;

}

class Tcp_socket;
class QFile;

class Torrent_properties_displayer : public QTabWidget {
	Q_OBJECT
public:
	explicit Torrent_properties_displayer(const magnet::Metadata & torrent_metadata, QWidget * parent = nullptr);
	explicit Torrent_properties_displayer(const bencode::Metadata & torrent_metadata, QWidget * parent = nullptr);

	void add_peer(const Tcp_socket * socket) noexcept;
	void remove_peer(std::int32_t peer_row_idx) noexcept;
	void update_file_info(qsizetype file_idx, std::int64_t file_dled_byte_cnt) noexcept;
	void setup_file_info_widget(const bencode::Metadata & torrent_metadata, const QList<std::pair<QFile *, std::int64_t>> & file_handles) noexcept;
	void display_file_bar() noexcept;

private:
	Torrent_properties_displayer(QWidget * parent = nullptr);

	void setup_general_info_widget(const bencode::Metadata & torrent_metadata) noexcept;
	void setup_peer_table() noexcept;
	QWidget * get_new_file_widget(const QString & file_path, std::int64_t total_file_size) noexcept;
	///
	QScrollArea file_info_scroll_area_;
	QWidget general_info_tab_;
	QWidget file_info_tab_;
	QTableWidget peer_table_;
	QFormLayout general_info_layout_{&general_info_tab_};
	QFormLayout file_info_layout_{&file_info_tab_};
};