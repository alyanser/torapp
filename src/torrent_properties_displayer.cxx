#include "torrent_properties_displayer.hxx"
#include "util.hxx"

#include <bencode_parser.hxx>
#include <QDesktopServices>
#include <QProgressBar>
#include <QPushButton>
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
         tab_widget_.addTab(&peer_widget_,"Peers");
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

QWidget * Torrent_properties_displayer::get_file_widget(const QString & file_path,const std::int64_t total_file_size) noexcept {
         auto * const file_widget = new QWidget(&file_info_widget_);
         auto * const file_layout = new QHBoxLayout(file_widget);

         auto * const file_dl_progress_bar = new QProgressBar();
         auto * const open_button = new QPushButton("Open");

         file_layout->addWidget(file_dl_progress_bar);
         file_layout->addWidget(open_button);

         file_dl_progress_bar->setMaximum(static_cast<std::int32_t>(total_file_size));
         open_button->setEnabled(false);

         assert(file_dl_progress_bar->parent());
         assert(open_button->parent());

         connect(file_dl_progress_bar,&QProgressBar::valueChanged,open_button,[file_dl_progress_bar,open_button](const auto new_value){

                  if(util::conversion::convert_to_percentile(new_value,file_dl_progress_bar->maximum()) == 100){
                           open_button->setEnabled(true);
                  }
         });

         connect(open_button,&QPushButton::clicked,&file_info_widget_,[file_path]{

                  if(!QDesktopServices::openUrl(file_path)){
                           constexpr std::string_view error_title("Could not open");
                           constexpr std::string_view error_body("Could not open the file. It may be deleted or corrupted");
                           QMessageBox::critical(nullptr,error_title.data(),error_body.data());
                  }
         });

         return file_widget;
}

void Torrent_properties_displayer::setup_file_info_widget(const bencode::Metadata & torrent_metadata,const QList<std::pair<QFile *,std::int64_t>> & file_handles) noexcept {
         assert(file_handles.size() == static_cast<qsizetype>(torrent_metadata.file_info.size()));

         for(qsizetype file_idx = 0;file_idx < file_handles.size();++file_idx){
                  const auto & [torrent_file_name,total_file_size] = torrent_metadata.file_info[static_cast<std::size_t>(file_idx)];
                  auto [file_handle,file_dl_byte_cnt] = file_handles[file_idx];
                  file_info_layout_.addRow(torrent_file_name.data(),get_file_widget(file_handle->fileName(),static_cast<std::int32_t>(total_file_size)));

                  update_file_info(file_idx,file_dl_byte_cnt);
         }
}

void Torrent_properties_displayer::update_file_info(const qsizetype file_idx,const std::int64_t file_dled_byte_cnt) noexcept {
         assert(file_idx >= 0 && file_idx < file_info_layout_.rowCount());

         auto * const file_dl_progress_bar = [&file_info_layout_ = file_info_layout_,file_idx]{
                  auto * const progress_bar_item = file_info_layout_.itemAt(static_cast<std::int32_t>(file_idx),QFormLayout::ItemRole::FieldRole);

                  assert(progress_bar_item);
                  assert(progress_bar_item->widget());
                  assert(progress_bar_item->widget()->layout());
                  assert(progress_bar_item->widget()->layout()->count() == 2);

                  constexpr auto progress_bar_idx = 0;
                  return qobject_cast<QProgressBar*>(progress_bar_item->widget()->layout()->itemAt(progress_bar_idx)->widget());
         }();

         assert(file_dl_progress_bar);

         file_dl_progress_bar->setValue(static_cast<std::int32_t>(file_dled_byte_cnt));
         file_dl_progress_bar->setFormat(QString("Downloaded: %1%").arg(util::conversion::convert_to_percentile(file_dled_byte_cnt,file_dl_progress_bar->maximum())));
}