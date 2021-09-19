#include "download_tracker.hxx"
#include "utility.hxx"

Download_tracker::Download_tracker(){
         setup_layout();
         setup_file_status_layout();
         setup_network_status_layout();
         setup_state_widget();
         update_error_line();
         configure_default_connections();

         time_elapsed_timer_.start(std::chrono::milliseconds(1000));
         open_button_.setEnabled(false);
         delete_button_.setEnabled(false);
}

Download_tracker::Download_tracker(const util::Download_request & download_request) : Download_tracker(){
         assert(!download_request.url.isEmpty());
         assert(!download_request.download_path.isEmpty());

         package_name_label_.setText(download_request.url.fileName());
         download_path_label_.setText(download_request.download_path);

         {
                  auto on_open_button_clicked = [this,path = download_request.download_path,name = download_request.package_name]{

                           if(!QDesktopServices::openUrl(path + name)){
                                    constexpr std::string_view message_title("Could not open file");
                                    constexpr std::string_view message_body("Downloaded file could not be opened");
				
                                    QMessageBox::warning(this,message_title.data(),message_body.data());
                           }
                  };

                  auto on_retry_button_clicked = [this,download_request = download_request]{
                           emit retry_url_download(download_request);
                           emit release_lifetime();
                  };

                  auto on_open_directory_button_clicked = [this,download_path = download_request.download_path]{

                           if(!QDesktopServices::openUrl(download_path)){
                                    constexpr std::string_view error_title("Directory open error");
                                    constexpr std::string_view error_body("Directory could not be opened");

                                    QMessageBox::critical(this,error_title.data(),error_body.data());
                           }
                  };

                  connect(&open_button_,&QPushButton::clicked,on_open_button_clicked);
                  connect(&open_directory_button_,&QPushButton::clicked,on_open_directory_button_clicked);
                  connect(&retry_button_,&QPushButton::clicked,this,on_retry_button_clicked,Qt::SingleShotConnection);
         }
}

void Download_tracker::setup_state_widget() noexcept {
         state_holder_.addWidget(&download_progress_bar_);
         state_holder_.addWidget(&error_line_);
         assert(state_holder_.currentWidget() == &download_progress_bar_);
         download_progress_bar_.setMinimum(0);
         download_progress_bar_.setValue(0);
         error_line_.setAlignment(Qt::AlignCenter);
}

void Download_tracker::setup_file_status_layout() noexcept {
         file_stat_layout_.addLayout(&package_name_layout_);
         file_stat_layout_.addLayout(&download_path_layout_);
         file_stat_layout_.addLayout(&time_elapsed_layout_);

         package_name_layout_.addWidget(&package_name_buddy_);
         package_name_layout_.addWidget(&package_name_label_);
         package_name_buddy_.setBuddy(&package_name_label_);

         download_path_layout_.addWidget(&download_path_buddy_);
         download_path_layout_.addWidget(&download_path_label_);
         download_path_layout_.addWidget(&open_directory_button_);
         download_path_buddy_.setBuddy(&download_path_label_);

         time_elapsed_layout_.addWidget(&time_elapsed_buddy_);
         time_elapsed_layout_.addWidget(&time_elapsed_label_);
         time_elapsed_buddy_.setBuddy(&time_elapsed_label_);
}

void Download_tracker::setup_network_status_layout() noexcept {
         network_stat_layout_.addLayout(&download_speed_layout_);
         network_stat_layout_.addLayout(&download_quantity_layout_);
         network_stat_layout_.addLayout(&upload_quantity_layout_);

         network_stat_layout_.addWidget(&delete_button_);
         network_stat_layout_.addWidget(&initiate_buttons_holder_);
         network_stat_layout_.addWidget(&terminate_buttons_holder_);

         download_speed_layout_.addWidget(&download_speed_buddy_);
         download_speed_layout_.addWidget(&download_speed_label_);
         download_speed_buddy_.setBuddy(&download_speed_label_);

         download_quantity_buddy_.setBuddy(&download_quantity_label_);
         download_quantity_layout_.addWidget(&download_quantity_buddy_);
         download_quantity_layout_.addWidget(&download_quantity_label_);

         upload_quantity_buddy_.setBuddy(&upload_quantity_label_);
         upload_quantity_layout_.addWidget(&upload_quantity_buddy_);
         upload_quantity_layout_.addWidget(&upload_quantity_label_);

         initiate_buttons_holder_.addWidget(&open_button_);
         initiate_buttons_holder_.addWidget(&retry_button_);

         terminate_buttons_holder_.addWidget(&cancel_button_);
         terminate_buttons_holder_.addWidget(&finish_button_);
}

void Download_tracker::download_progress_update(const int64_t bytes_received,const int64_t total_bytes) noexcept {
         assert(bytes_received >= 0);
         assert(!download_progress_bar_.minimum());

         constexpr auto unknown_bytes = -1;

         if(total_bytes == unknown_bytes){
                  // sets the bar in pending state
                  download_progress_bar_.setMaximum(0);
                  download_progress_bar_.setValue(0);
         }else{
                  download_progress_bar_.setMaximum(static_cast<int32_t>(total_bytes));
                  download_progress_bar_.setValue(static_cast<int32_t>(bytes_received));
         }

         download_quantity_label_.setText(util::conversion::stringify_bytes(bytes_received,total_bytes));

         const auto seconds_elapsed = time_elapsed_.second() + time_elapsed_.minute() * 60 + time_elapsed_.hour() * 3600;
         assert(seconds_elapsed > 0);
         const auto speed = bytes_received / seconds_elapsed;
	constexpr auto conversion_format = util::conversion::Conversion_Format::Speed;
         const auto [converted_speed,speed_postfix] = stringify_bytes(static_cast<double>(speed),conversion_format);

         download_speed_label_.setText(QString("%1 %2").arg(converted_speed).arg(speed_postfix.data()));
}


void Download_tracker::configure_default_connections() noexcept {

         auto on_cancel_button_clicked = [this]{
                  constexpr std::string_view question_title("Cancel Download");
                  constexpr std::string_view question_body("Are you sure you want to cancel the download?");
                  constexpr auto buttons = QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No;
                  
                  const auto response = QMessageBox::question(this,question_title.data(),question_body.data(),buttons);

                  if(response == QMessageBox::StandardButton::Yes){
                           emit request_satisfied();
                  }
         };

         auto on_delete_button_clicked = [this]{
                  QMessageBox query_box(QMessageBox::Icon::NoIcon,"Delete file","",QMessageBox::NoButton);

                  auto * const delete_permanently_button = query_box.addButton("Delete permanently",QMessageBox::ButtonRole::DestructiveRole);
                  auto * const move_to_trash_button = query_box.addButton("Move to Trash",QMessageBox::ButtonRole::YesRole);
                  [[maybe_unused]] auto * const cancel_button = query_box.addButton("Cancel",QMessageBox::ButtonRole::RejectRole);

                  connect(delete_permanently_button,&QPushButton::clicked,this,&Download_tracker::delete_file_permanently);
                  connect(move_to_trash_button,&QPushButton::clicked,this,&Download_tracker::move_file_to_trash);
                  connect(this,&Download_tracker::delete_file_permanently,this,&Download_tracker::release_lifetime);
                  connect(this,&Download_tracker::move_file_to_trash,this,&Download_tracker::release_lifetime);
                  
                  [[maybe_unused]] const auto response = query_box.exec();
         };

         connect(&time_elapsed_timer_,&QTimer::timeout,[&time_elapsed_ = time_elapsed_,&time_elapsed_label_ = time_elapsed_label_]{
                  time_elapsed_ = time_elapsed_.addSecs(1);
                  time_elapsed_label_.setText(time_elapsed_.toString() + " hh:mm::ss");
         });

         connect(&delete_button_,&QPushButton::clicked,on_delete_button_clicked);
         connect(&cancel_button_,&QPushButton::clicked,this,on_cancel_button_clicked);
         connect(&finish_button_,&QPushButton::clicked,this,&Download_tracker::request_satisfied);
}

void Download_tracker::switch_to_finished_state() noexcept {
         time_elapsed_timer_.stop();
         time_elapsed_buddy_.setText("Time took: ");
         terminate_buttons_holder_.setCurrentWidget(&finish_button_);
         state_holder_.setCurrentWidget(&error_line_);
         
         if(error_ == Error::Null){
                  delete_button_.setEnabled(true);
                  open_button_.setEnabled(true);
         }else{
                  initiate_buttons_holder_.setCurrentWidget(&retry_button_);
         }
}

void Download_tracker::update_error_line() noexcept {
         constexpr std::string_view null_error_info("Download completed successfully. Press the open button to view");
         constexpr std::string_view file_write_error_info("Given file could not be opened for writing");
         constexpr std::string_view unknown_network_error_info("Unknown network error. Try restarting the download");
         constexpr std::string_view file_lock_error_info("Same file is held by another download. Cancel that download and retry");

         switch(error_){
                  case Error::Null :
			error_line_.setText(null_error_info.data()); 
			break;
                  case Error::File_Write :
			error_line_.setText(file_write_error_info.data()); 
			break;
                  case Error::Unknown_Network :
			error_line_.setText(unknown_network_error_info.data()); 
			break;
                  case Error::File_Lock :
			error_line_.setText(file_lock_error_info.data()); 
			break;
                  case Error::Custom : [[fallthrough]];
                  default : __builtin_unreachable();
         }
}