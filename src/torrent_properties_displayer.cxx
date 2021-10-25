#include "torrent_properties_displayer.hxx"
#include "util.hxx"

#include <bencode_parser.hxx>
#include <QProgressBar>
#include <QLabel>

Torrent_properties_displayer::Torrent_properties_displayer(const bencode::Metadata & torrent_metadata,QWidget * const parent)
         : QWidget(parent)
{
         setWindowTitle(QString("Torrent Information") + (torrent_metadata.name.empty() ? "" : '(' + QString(torrent_metadata.name.data()) + ')'));

         setup_tab_widget();
         setup_general_info_widget(torrent_metadata);
}

void Torrent_properties_displayer::setup_tab_widget() noexcept {
         tab_widget_.addTab(&general_info_widget_,"General");
         tab_widget_.addTab(&file_info_widget_,"Files");
}

void Torrent_properties_displayer::setup_general_info_widget(const bencode::Metadata & torrent_metadata) noexcept {
         general_info_layout_.addRow("Name:",new QLabel(torrent_metadata.name.empty() ? "N/A" : torrent_metadata.name.data()));
         general_info_layout_.addRow("Created By:",new QLabel(torrent_metadata.created_by.empty() ? "N/A" : torrent_metadata.created_by.data()));
         general_info_layout_.addRow("Creation Date:",new QLabel(torrent_metadata.creation_date.empty() ? "N/A" : torrent_metadata.creation_date.data()));
         general_info_layout_.addRow("Comment:",new QLabel(torrent_metadata.comment.empty() ? "N/A" : torrent_metadata.comment.data()));
         general_info_layout_.addRow("Encoding:",new QLabel(torrent_metadata.encoding.empty() ? "N/A" : torrent_metadata.encoding.data()));
         general_info_layout_.addRow("Md5sum:",new QLabel(torrent_metadata.md5sum.empty() ? "N/A" : torrent_metadata.md5sum.data()));
         general_info_layout_.addRow("Piece Size:",new QLabel(QString::number(torrent_metadata.piece_length)));
}

void Torrent_properties_displayer::setup_file_info_widget(const bencode::Metadata & torrent_metadata,const QList<std::pair<QFile *,std::int64_t>> & file_handles) noexcept {
         assert(file_handles.size() == static_cast<qsizetype>(torrent_metadata.file_info.size()));

         for(qsizetype file_idx = 0;file_idx < file_handles.size();++file_idx){
                  const auto & [file_handle,file_dled_byte_cnt] = file_handles[file_idx];
                  qDebug() << file_dled_byte_cnt;
                  auto * const file_dl_progress_bar = new QProgressBar();

                  // ! consider the overflow
                  file_info_layout_.addRow(torrent_metadata.file_info[file_idx].first.data(),file_dl_progress_bar);
                  assert(file_dl_progress_bar->parent());

                  const auto total_file_size = static_cast<std::int32_t>(torrent_metadata.file_info[static_cast<std::size_t>(file_idx)].second);
                  file_dl_progress_bar->setMaximum(total_file_size);

                  update_file_info(file_idx,file_dled_byte_cnt);
         }
}

void Torrent_properties_displayer::update_file_info(const qsizetype file_idx,const std::int64_t file_dled_byte_cnt) noexcept {
         assert(file_idx >= 0 && file_idx < file_info_layout_.rowCount());
         auto * const file_dl_progress_bar = qobject_cast<QProgressBar*>(file_info_layout_.itemAt(file_idx,QFormLayout::ItemRole::FieldRole)->widget());
         assert(file_dl_progress_bar);
         file_dl_progress_bar->setValue(file_dled_byte_cnt);
         file_dl_progress_bar->setFormat(QString("Downloaded: %1%").arg(util::conversion::convert_to_percentile(file_dled_byte_cnt,file_dl_progress_bar->maximum())));
}