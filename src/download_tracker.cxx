#include "download_tracker.hxx"

#include <bencode_parser.hxx>
#include <QDesktopServices>
#include <QMessageBox>
#include <QDir>

Download_tracker::Download_tracker(const QString & dl_path,QWidget * const parent) 
         : QWidget(parent)
{
         setup_layout();
         setup_file_status_layout();
         setup_network_status_layout();
         setup_state_widget();
         update_error_line();
         configure_default_connections();

         time_elapsed_timer_.start(std::chrono::seconds(1));
         open_button_.setEnabled(false);
         delete_button_.setEnabled(false);

         {
                  connect(&open_dir_button_,&QPushButton::clicked,this,[this,dl_path]{
                           assert(dl_path.lastIndexOf('/') != -1);
                           //! doesn't show the error
                           if(!QDesktopServices::openUrl(dl_path.sliced(0,dl_path.lastIndexOf('/') + 1))){
                                    constexpr std::string_view error_title("Directory open error");
                                    constexpr std::string_view error_body("Directory could not be opened");
                                    QMessageBox::critical(this,error_title.data(),error_body.data());
                           }
                  });

                  connect(&open_button_,&QPushButton::clicked,this,[this,dl_path]{

                           if(!QDesktopServices::openUrl(dl_path)){
                                    constexpr std::string_view message_title("Could not open file");
                                    constexpr std::string_view message_body("Downloaded file could not be opened");
                                    QMessageBox::critical(this,message_title.data(),message_body.data());
                           }
                  });
         }
}

Download_tracker::Download_tracker(const QString & path,const QUrl url,QWidget * const parent) 
         : Download_tracker(path,parent)
{
         assert(!url.isEmpty());
         assert(!path.isEmpty());
         
         package_name_label_.setText(url.fileName());
         dl_path_label.setText(path);

         connect(&retry_button_,&QPushButton::clicked,this,[this,path,url]{
                  emit retry_download(path,url);
                  emit request_satisfied();
         });
}

Download_tracker::Download_tracker(const QString & path,bencode::Metadata torrent_metadata,QWidget * const parent) 
         : Download_tracker(path,parent)
{
         package_name_label_.setText(torrent_metadata.name.data());

         connect(&retry_button_,&QPushButton::clicked,this,[this,path,torrent_metadata = std::move(torrent_metadata)]{
                  emit retry_download(path,torrent_metadata);
                  emit request_satisfied();
         });
}

void Download_tracker::setup_state_widget() noexcept {
         state_holder_.addWidget(&error_line_);
         
         dl_progress_bar_.setMinimum(0);
         dl_progress_bar_.setValue(0);

         error_line_.setAlignment(Qt::AlignCenter);
         assert(state_holder_.currentWidget() == &dl_progress_bar_);
}

void Download_tracker::setup_file_status_layout() noexcept {
         file_stat_layout_.addLayout(&package_name_layout_);
         file_stat_layout_.addLayout(&dl_path_layout_);
         file_stat_layout_.addLayout(&time_elapsed_layout_);

         package_name_layout_.addWidget(&package_name_buddy_);
         package_name_layout_.addWidget(&package_name_label_);
         package_name_buddy_.setBuddy(&package_name_label_);

         dl_path_layout_.addWidget(&dl_path_buddy_);
         dl_path_layout_.addWidget(&dl_path_label);
         dl_path_layout_.addWidget(&open_dir_button_);
         dl_path_buddy_.setBuddy(&dl_path_label);

         state_holder_.addWidget(&dl_progress_bar_);
         time_elapsed_layout_.addWidget(&time_elapsed_buddy_);
         time_elapsed_layout_.addWidget(&time_elapsed_label_);
         time_elapsed_buddy_.setBuddy(&time_elapsed_label_);
}

void Download_tracker::setup_network_status_layout() noexcept {
         network_stat_layout_.addLayout(&dl_speed_layout_);
         network_stat_layout_.addLayout(&dl_quanitity_layout_);
         network_stat_layout_.addLayout(&ul_quantity_layout_);

         network_stat_layout_.addWidget(&delete_button_);
         network_stat_layout_.addWidget(&initiate_buttons_holder_);
         network_stat_layout_.addWidget(&terminate_buttons_holder_);

         dl_speed_layout_.addWidget(&dl_speed_buddy_);
         dl_speed_layout_.addWidget(&dl_speed_label_);
         dl_speed_buddy_.setBuddy(&dl_speed_label_);

         dl_quantity_buddy_.setBuddy(&dl_quantity_label_);
         dl_quanitity_layout_.addWidget(&dl_quantity_buddy_);
         dl_quanitity_layout_.addWidget(&dl_quantity_label_);

         ul_quantity_buddy_.setBuddy(&ul_quanitity_label_);
         ul_quantity_layout_.addWidget(&ul_quantity_buddy_);
         ul_quantity_layout_.addWidget(&ul_quanitity_label_);

         initiate_buttons_holder_.addWidget(&open_button_);
         initiate_buttons_holder_.addWidget(&retry_button_);

         terminate_buttons_holder_.addWidget(&cancel_button_);
         terminate_buttons_holder_.addWidget(&finish_button_);
}

void Download_tracker::download_progress_update(std::int64_t received_byte_cnt,const std::int64_t total_byte_cnt) noexcept {
         received_byte_cnt += dl_byte_offset_;
         assert(received_byte_cnt >= 0);
         assert(!dl_progress_bar_.minimum());

         if(constexpr auto unknown_byte_cnt = -1;total_byte_cnt == unknown_byte_cnt){
                  // sets the bar in pending state
                  dl_progress_bar_.setMaximum(0);
                  dl_progress_bar_.setValue(0);
         }else{
                  // ! consider the overflow
                  dl_progress_bar_.setMaximum(static_cast<std::int32_t>(total_byte_cnt));
                  dl_progress_bar_.setValue(static_cast<std::int32_t>(received_byte_cnt));
         }

         dl_quantity_label_.setText(util::conversion::stringify_bytes(received_byte_cnt,total_byte_cnt));

         const auto seconds_elapsed = time_elapsed_.second() + time_elapsed_.minute() * 60 + time_elapsed_.hour() * 3600;
         assert(seconds_elapsed > 0);
         const auto speed = received_byte_cnt / seconds_elapsed;

         constexpr auto conversion_format = util::conversion::Conversion_Format::Speed;
         const auto [converted_speed,speed_postfix] = stringify_bytes(static_cast<double>(speed),conversion_format);

         dl_speed_label_.setText(QString("%1 %2").arg(converted_speed).arg(speed_postfix.data()));
}

void Download_tracker::configure_default_connections() noexcept {
         connect(&finish_button_,&QPushButton::clicked,this,&Download_tracker::download_dropped);
         connect(&finish_button_,&QPushButton::clicked,this,&Download_tracker::request_satisfied);
         connect(this,&Download_tracker::request_satisfied,&Download_tracker::deleteLater);

         connect(&delete_button_,&QPushButton::clicked,this,[this]{
                  QMessageBox query_box(QMessageBox::Icon::NoIcon,"Delete file","",QMessageBox::Button::NoButton);

                  auto * const delete_permanently_button = query_box.addButton("Delete permanently",QMessageBox::ButtonRole::DestructiveRole);
                  auto * const move_to_trash_button = query_box.addButton("Move to Trash",QMessageBox::ButtonRole::YesRole);
                  query_box.addButton("Cancel",QMessageBox::ButtonRole::RejectRole);

                  connect(delete_permanently_button,&QPushButton::clicked,this,&Download_tracker::delete_file_permanently);
                  connect(move_to_trash_button,&QPushButton::clicked,this,&Download_tracker::move_file_to_trash);
                  connect(this,&Download_tracker::delete_file_permanently,this,&Download_tracker::download_dropped);
                  connect(this,&Download_tracker::move_file_to_trash,this,&Download_tracker::download_dropped);
                  connect(this,&Download_tracker::delete_file_permanently,this,&Download_tracker::request_satisfied);
                  connect(this,&Download_tracker::move_file_to_trash,&Download_tracker::request_satisfied);

                  query_box.exec();
         });

         connect(&cancel_button_,&QPushButton::clicked,this,[this]{
                  constexpr std::string_view question_title("Cancel Download");
                  constexpr std::string_view question_body("Are you sure you want to cancel the download?");
                  constexpr auto buttons = QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No;
                  
                  const auto response_button = QMessageBox::question(this,question_title.data(),question_body.data(),buttons);

                  if(response_button == QMessageBox::StandardButton::Yes){
                           emit download_dropped();
                           emit request_satisfied();
                  }
         });

         time_elapsed_timer_.callOnTimeout(this,[&time_elapsed_ = time_elapsed_,&time_elapsed_label_ = time_elapsed_label_]{
                  time_elapsed_ = time_elapsed_.addSecs(1);
                  time_elapsed_label_.setText(time_elapsed_.toString() + " hh:mm::ss");
         });
}

void Download_tracker::switch_to_finished_state() noexcept {
         time_elapsed_timer_.stop();
         time_elapsed_buddy_.setText("Time took: ");
         state_holder_.setCurrentWidget(&error_line_);
         terminate_buttons_holder_.setCurrentWidget(&finish_button_);
         
         if(static_cast<bool>(error_)){
                  initiate_buttons_holder_.setCurrentWidget(&retry_button_);
         }else{
                  delete_button_.setEnabled(true);
                  open_button_.setEnabled(true);
         }
}

void Download_tracker::update_error_line() noexcept {
         constexpr std::string_view null_desc("Download completed successfully. Click on Open button to view");
         constexpr std::string_view file_write_desc("Given file could not be opened for writing.");
         constexpr std::string_view unknown_network_desc("Unknown network error. Try restarting the download");
         constexpr std::string_view file_lock_desc("Given path already exists. Cannot overwrite. Delete that and try again");

         switch(error_){
                  
                  case Error::Null : {
                           error_line_.setText(null_desc.data()); 
                           break;
                  }

                  case Error::File_Write : {
                           error_line_.setText(file_write_desc.data()); 
                           break;
                  }

                  case Error::Unknown_Network : {
                           error_line_.setText(unknown_network_desc.data()); 
                           break;
                  }
                  
                  case Error::File_Lock : { 
                           error_line_.setText(file_lock_desc.data()); 
                           break;
                  }

                  case Error::Custom : {
                           [[fallthrough]];
                  }

                  default : {
                           __builtin_unreachable();
                  }
         }
}