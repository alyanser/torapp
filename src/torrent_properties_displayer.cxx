#include "torrent_properties_displayer.hxx"
#include "tcp_socket.hxx"
#include "util.hxx"

#include <bencode_parser.hxx>
#include <QDesktopServices>
#include <QProgressBar>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QFile>

Torrent_properties_displayer::Torrent_properties_displayer(const bencode::Metadata & torrent_metadata,QWidget * const parent)
         : QWidget(parent)
{
         setWindowTitle(QString("Torrent Information") + (torrent_metadata.name.empty() ? "" : '(' + QString(torrent_metadata.name.data()) + ')'));
         setup_tab_widget();
         setup_general_info_widget(torrent_metadata);
         setup_peer_table();
}

void Torrent_properties_displayer::setup_tab_widget() noexcept {
         tab_widget_.addTab(&general_info_tab_,"General");
         tab_widget_.addTab(&file_info_tab_,"Files");
         tab_widget_.addTab(&peer_table_,"Peers");
}

void Torrent_properties_displayer::add_peer(Tcp_socket * const socket) noexcept {
         assert(peer_table_.columnCount() == 4);

         peer_table_.setRowCount((peer_table_.rowCount() + 1));
         const auto row_idx = peer_table_.rowCount() - 1;

         auto get_cell_label_text = [](const auto byte_cnt,const auto conversion_fmt){
                  const auto [converted_byte_cnt,suffix] = util::conversion::stringify_bytes(byte_cnt,conversion_fmt);
                  return QString::number(converted_byte_cnt) + ' ' + suffix.data();
         };

         auto * const dled_byte_cnt_label = [socket,get_cell_label_text]{
                  auto * const dled_byte_cnt_label = new QLabel(get_cell_label_text(0,util::conversion::Format::Memory));
                  dled_byte_cnt_label->setAlignment(Qt::AlignCenter);
                  
                  connect(socket,&Tcp_socket::downloaded_byte_count_changed,dled_byte_cnt_label,[=](const auto dled_byte_cnt){
                           dled_byte_cnt_label->setText(get_cell_label_text(dled_byte_cnt,util::conversion::Format::Memory));
                  });

                  return dled_byte_cnt_label;
         }();

         auto * const uled_byte_cnt_label = [socket,get_cell_label_text]{
                  auto * const uled_byte_cnt_label = new QLabel(get_cell_label_text(0,util::conversion::Format::Memory));
                  uled_byte_cnt_label->setAlignment(Qt::AlignCenter);

                  connect(socket,&Tcp_socket::uploaded_byte_count_changed,uled_byte_cnt_label,[=](const auto uled_byte_cnt){
                           uled_byte_cnt_label->setText(get_cell_label_text(uled_byte_cnt,util::conversion::Format::Memory));
                  });

                  return uled_byte_cnt_label;
         }();

         auto * const dl_speed_label = [get_cell_label_text,socket]{
                  auto * const dl_speed_label = new QLabel(get_cell_label_text(0,util::conversion::Format::Speed));
                  dl_speed_label->setAlignment(Qt::AlignCenter);

                  {
                           auto * const speed_refresh_timer = new QTimer(socket);
                           speed_refresh_timer->setInterval(std::chrono::seconds(1));

                           speed_refresh_timer->callOnTimeout(socket,[=,seconds_elapsed = 0LL]() mutable {
                                    dl_speed_label->setText(get_cell_label_text(socket->downloaded_byte_count() / ++seconds_elapsed,util::conversion::Format::Speed));
                           });

                           speed_refresh_timer->start();
                  }

                  return dl_speed_label;
         }();

         auto * const peer_id_label = [peer_id = QByteArray::fromHex(socket->peer_id)]{
                  auto * const peer_id_label = new QLabel(peer_id);
                  peer_id_label->setAlignment(Qt::AlignCenter);
                  return peer_id_label;
         }();

         constexpr auto peer_id_col_idx = 0;
         constexpr auto dled_byte_col_idx = 1;
         constexpr auto uled_byte_col_idx = 2;
         constexpr auto dl_speed_col_idx = 3;

         peer_table_.setCellWidget(row_idx,peer_id_col_idx,peer_id_label);
         peer_table_.setCellWidget(row_idx,dled_byte_col_idx,dled_byte_cnt_label);
         peer_table_.setCellWidget(row_idx,uled_byte_col_idx,uled_byte_cnt_label);
         peer_table_.setCellWidget(row_idx,dl_speed_col_idx,dl_speed_label);

         assert(dled_byte_cnt_label->parent());
}

void Torrent_properties_displayer::setup_general_info_widget(const bencode::Metadata & torrent_metadata) noexcept {
         general_info_layout_.addRow("Name:",new QLabel(torrent_metadata.name.empty() ? "N/A" : torrent_metadata.name.data()));
         general_info_layout_.addRow("Created By:",new QLabel(torrent_metadata.created_by.empty() ? "N/A" : torrent_metadata.created_by.data()));
         general_info_layout_.addRow("Creation Date:",new QLabel(torrent_metadata.creation_date.empty() ? "N/A" : torrent_metadata.creation_date.data()));
         general_info_layout_.addRow("Comment:",new QLabel(torrent_metadata.comment.empty() ? "N/A" : torrent_metadata.comment.data()));
         general_info_layout_.addRow("Encoding:",new QLabel(torrent_metadata.encoding.empty() ? "N/A" : torrent_metadata.encoding.data()));
         general_info_layout_.addRow("Md5-Sum:",new QLabel(torrent_metadata.md5sum.empty() ? "N/A" : torrent_metadata.md5sum.data()));
         general_info_layout_.addRow("Piece Size:",new QLabel(QString::number(torrent_metadata.piece_length) + " bytes"));
         // todo: add progress bars
}

void Torrent_properties_displayer::setup_peer_table() noexcept {
         const QList<QString> peer_table_headings {"Peer Id","Downloaded","Uploaded","Download Speed"};
         peer_table_.setColumnCount(static_cast<std::int32_t>(peer_table_headings.size()));
         peer_table_.setHorizontalHeaderLabels(peer_table_headings);
}

[[nodiscard]]
QWidget * Torrent_properties_displayer::get_new_file_widget(const QString & file_path,const std::int64_t total_file_size) noexcept {
         auto * const file_widget = new QWidget(&file_info_tab_);
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
                  assert(util::conversion::convert_to_percentile(new_value,file_dl_progress_bar->maximum()) <= 100);

                  if(util::conversion::convert_to_percentile(new_value,file_dl_progress_bar->maximum()) == 100){
                           open_button->setEnabled(true);
                  }
         });

         connect(open_button,&QPushButton::clicked,&file_info_tab_,[file_path]{

                  if(!QDesktopServices::openUrl(file_path)){
                           constexpr std::string_view error_title("Could not open");
                           constexpr std::string_view error_body("Could not open the file. It may have been deleted or corrupted");
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
                  file_info_layout_.addRow(torrent_file_name.data(),get_new_file_widget(file_handle->fileName(),static_cast<std::int32_t>(total_file_size)));

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

         assert(file_dl_progress_bar->maximum());
         file_dl_progress_bar->setValue(static_cast<std::int32_t>(file_dled_byte_cnt));
         assert(file_dl_progress_bar->maximum());
         file_dl_progress_bar->setFormat(QString("Downloaded: %1%").arg(util::conversion::convert_to_percentile(file_dled_byte_cnt,file_dl_progress_bar->maximum())));
}