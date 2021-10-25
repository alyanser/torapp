#include "torrent_properties_displayer.hxx"

#include <bencode_parser.hxx>
#include <QProgressBar>
#include <QLabel>

Torrent_properties_displayer::Torrent_properties_displayer(const bencode::Metadata & torrent_metadata,QWidget * const parent)
         : QWidget(parent)
{
         setWindowTitle(QString("Torrent Information") + (torrent_metadata.name.empty() ? "" : '(' + QString(torrent_metadata.name.data()) + ')'));

         setup_tab_widget();
         setup_general_info_widget(torrent_metadata);
         setup_file_info_widget(torrent_metadata);
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

void Torrent_properties_displayer::setup_file_info_widget(const bencode::Metadata & torrent_metadata) noexcept {

         std::for_each(torrent_metadata.file_info.begin(),torrent_metadata.file_info.end(),[&file_info_layout_ = file_info_layout_](const auto & file_info){
                  const auto & [file_name,file_size] = file_info;

                  auto * const file_progress_bar = new QProgressBar();

                  file_progress_bar->setValue(0);
                  file_progress_bar->setMaximum(file_size);
                  file_progress_bar->setFormat(QString("Downloaded: %1%").arg(0));
                  
                  file_info_layout_.addRow(file_name.data(),file_progress_bar);
                  assert(file_progress_bar->parent());
         });
}

void Torrent_properties_displayer::update_file_info(const QString & file_name,const std::int64_t dled_byte_cnt) noexcept {

         auto * const file_progress_bar = [&file_name,&file_idxes_ = file_idxes_,&file_info_layout_ = file_info_layout_]{
                  assert(file_idxes_.contains(file_name));

                  const auto file_info_idx = file_idxes_[file_name];
                  assert(file_info_idx < file_info_layout_.rowCount());

                  return qobject_cast<QProgressBar*>(file_info_layout_.itemAt(file_info_idx)->widget());
         }();

         assert(file_progress_bar);
         assert(dled_byte_cnt <= file_progress_bar->maximum());
         
         file_progress_bar->setValue(dled_byte_cnt);
}