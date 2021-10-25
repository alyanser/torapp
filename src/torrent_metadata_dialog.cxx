#include "torrent_metadata_dialog.hxx"

#include <QStorageInfo>

void Torrent_metadata_dialog::setup_layout() noexcept {
         central_layout_.addLayout(&central_form_layout_,0,0);
         
         central_form_layout_.setSpacing(10);
         
         central_form_layout_.addRow("Name",&torrent_name_label_);
         central_form_layout_.addRow("Created by",&created_by_label_);
         central_form_layout_.addRow("Creation date",&creation_date_label_);
         central_form_layout_.addRow("Announce",&announce_label_);
         central_form_layout_.addRow("Comment",&comment_label_);
         central_form_layout_.addRow("Encoding",&encoding_label_);
         central_form_layout_.addRow("Size",&torrent_length_label_);
         central_form_layout_.addRow(&file_form_layout_);
         central_form_layout_.addRow("Download directory",&path_layout_);
         central_form_layout_.addRow(&button_layout_);

         file_form_layout_.addRow("Files",&file_layout_);
         
         button_layout_.addWidget(&begin_download_button_);
         button_layout_.addWidget(&cancel_button_);

         path_layout_.addWidget(&path_line_);
         path_layout_.addWidget(&path_button_);
}

void Torrent_metadata_dialog::setup_display(const bencode::Metadata & torrent_metadata) noexcept {
         torrent_name_label_.setText(torrent_metadata.name.data());
         torrent_length_label_.setText(QString::number(torrent_metadata.piece_length) + " kbs");
         comment_label_.setText(torrent_metadata.comment.data());
         created_by_label_.setText(torrent_metadata.created_by.data());
         creation_date_label_.setText(torrent_metadata.creation_date.data());
         comment_label_.setText(torrent_metadata.comment.data());
         encoding_label_.setText(torrent_metadata.encoding.data());

         std::for_each(torrent_metadata.file_info.cbegin(),torrent_metadata.file_info.cend(),[this](const auto & file_info){
                  const auto & [file_path,file_size_kbs] = file_info;

                  const auto file_size_byte_cnt = file_size_kbs * 1024;
                  const auto [converted_size,postfix] = util::conversion::stringify_bytes(file_size_byte_cnt,util::conversion::Format::Memory);

                  auto * const file_label = new QLabel(QString(file_path.data()) + '\t' + QString::number(converted_size) + postfix.data(),this);
                  file_layout_.addWidget(file_label);
         });
}

void Torrent_metadata_dialog::extract_metadata(const QString & torrent_file_path) noexcept {

         auto torrent_metadata = [&torrent_file_path]() -> std::optional<bencode::Metadata> {

                  try{
                           return bencode::extract_metadata(bencode::parse_file(torrent_file_path.toStdString()));

                  }catch(const std::exception & exception){
                           qDebug() << exception.what();
                           return {};
                  }
         }();

         if(!torrent_metadata){
                  // todo: report to tracker
                  qDebug() << "Could not parse the given file";
                  return;
         }

         setup_display(*torrent_metadata);

         connect(&begin_download_button_,&QPushButton::clicked,this,[this,torrent_metadata = std::move(torrent_metadata)]{

                  const auto dir_path = [path_line_text = path_line_.text(),&torrent_metadata]() mutable {

                           if(!path_line_text.isEmpty() && path_line_text.back() != '/'){
                                    path_line_text.push_back('/');
                           }

                           return path_line_text + torrent_metadata->name.data();
                  }();
                  
                  if(dir_path.isEmpty()){
                           constexpr std::string_view error_title("Invalid path");
                           constexpr std::string_view error_body("Path cannot be empty");

                           QMessageBox::critical(this,error_title.data(),error_body.data());
                           return;
                  }

                  if(QFileInfo::exists(dir_path)){
                           constexpr std::string_view query_title("Already exists");
                           constexpr std::string_view query_body("Directory already exists. Do you wish to replace it?");

                           const auto reply_button = QMessageBox::question(this,query_title.data(),query_body.data());

                           if(reply_button == QMessageBox::No){
                                    return;
                           }
                  }

                  {
                           QStorageInfo storage(path_line_.text());
                           assert(storage.isValid());
                           
                           if(storage.bytesFree() < (torrent_metadata->single_file ? torrent_metadata->single_file_size : torrent_metadata->multiple_files_size)){
                                    constexpr std::string_view error_title("Not enough space");
                                    constexpr std::string_view error_body("Not enough space available in the specified directory. Choose another path and retry");

                                    QMessageBox::critical(this,error_title.data(),error_body.data());
                                    return;
                           }
                  }


                  accept();
                  emit new_request_received(dir_path,*torrent_metadata);
         });
}