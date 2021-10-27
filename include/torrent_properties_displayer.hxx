#pragma once

#include <QTableWidget>
#include <QFormLayout>
#include <QTabWidget>
#include <QWidget>

class QFile;

namespace bencode {
         struct Metadata;
}

class Tcp_socket;

class Torrent_properties_displayer : public QTabWidget {
         Q_OBJECT
public:
         explicit Torrent_properties_displayer(const bencode::Metadata & torrent_metadata,QWidget * parent = nullptr);
         
         void add_peer(const Tcp_socket * socket) noexcept;
         void update_file_info(qsizetype file_idx,std::int64_t dled_byte_cnt) noexcept;
         void setup_file_info_widget(const bencode::Metadata & torrent_metadata,const QList<std::pair<QFile *,std::int64_t>> & file_handles) noexcept;
private:
         void setup_general_info_widget(const bencode::Metadata & torrent_metadata) noexcept;
         void setup_peer_table() noexcept;
         QWidget * get_new_file_widget(const QString & file_path,std::int64_t total_file_size) noexcept;
         ///
         QWidget general_info_tab_;
         QWidget file_info_tab_;
         QTableWidget peer_table_;
         QFormLayout general_info_layout_{&general_info_tab_};
         QFormLayout file_info_layout_{&file_info_tab_};
};