#pragma once

#include <QFormLayout>
#include <QTabWidget>
#include <QWidget>

namespace bencode {
         struct Metadata;
}

class Torrent_properties_displayer : public QWidget {
         Q_OBJECT
public:
         explicit Torrent_properties_displayer(const bencode::Metadata & torrent_metadata,QWidget * parent = nullptr);
         void update_file_info(qsizetype file_idx,std::int64_t dled_byte_cnt) noexcept;
         void setup_file_info_widget(const bencode::Metadata & torrent_metadata,const QList<std::pair<QFile *,std::int64_t>> & file_handles) noexcept;
private:
         void setup_tab_widget() noexcept;
         void setup_general_info_widget(const bencode::Metadata & torrent_metadata) noexcept;
         QWidget * get_file_widget(const QString & file_path,std::int64_t total_file_size) noexcept;
         ///
         QTabWidget tab_widget_{this};
         QWidget general_info_widget_{this};
         QWidget file_info_widget_{this};
         QWidget peer_widget_{this};
         QFormLayout general_info_layout_{&general_info_widget_};
         QFormLayout file_info_layout_{&file_info_widget_};
};