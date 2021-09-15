#include "download_tracker.hxx"
#include "download_request.hxx"

Download_tracker::Download_tracker(const Download_request & download_request){
         assert(!download_request.url.isEmpty());
         assert(!download_request.download_path.isEmpty());

         setup_layout();
         setup_file_status_layout();
         setup_network_status_layout();
         setup_state_widget();
         update_state_line();
         configure_default_connections();

         package_name_label_.setText(download_request.url.fileName());
         download_path_label_.setText(download_request.download_path);
         time_elapsed_timer_.start(std::chrono::milliseconds(1000));
         open_button_.setEnabled(false);
         delete_button_.setEnabled(false);

         {
                  auto on_open_button_clicked = [this,path = download_request.download_path,name = download_request.package_name]{

                           if(!QDesktopServices::openUrl(path + name)){
                                    constexpr std::string_view message_title("Could not open file");
                                    constexpr std::string_view message_body("Downloaded file could not be opened");

                                    QMessageBox::warning(this,message_title.data(),message_body.data());
                           }
                  };

                  auto on_retry_button_clicked = [this,download_request = download_request]() mutable {
                           download_request.retry_existing_request = true;
                           
                           emit retry_download(download_request);
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

QString Download_tracker::stringify_bytes(int64_t bytes_received,int64_t total_bytes) noexcept {
         constexpr auto format = Conversion_Format::Memory;
         constexpr auto unknown_bound = -1;
         double converted_total_bytes = 0;
         std::string_view total_bytes_postfix("inf");

         if(total_bytes != unknown_bound){
                  std::tie(converted_total_bytes,total_bytes_postfix) = stringify_bytes(static_cast<double>(total_bytes),format);
         }

         const auto [converted_received_bytes,received_bytes_postfix] = stringify_bytes(static_cast<double>(bytes_received),format);
         QString quantity_text("%1 %2 / %3 %4");

         quantity_text = quantity_text.arg(converted_received_bytes).arg(received_bytes_postfix.data());
         quantity_text = quantity_text.arg(converted_total_bytes).arg(total_bytes_postfix.data());

         return quantity_text;
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
                  // download_progress_bar_.setValue(0); //? necessary?
         }else{
                  download_progress_bar_.setMaximum(static_cast<int32_t>(total_bytes));
                  download_progress_bar_.setValue(static_cast<int32_t>(bytes_received));
         }

         download_quantity_label_.setText(stringify_bytes(bytes_received,total_bytes));

         const auto seconds_elapsed = time_elapsed_.second() + time_elapsed_.minute() * 60 + time_elapsed_.hour() * 3600;
         assert(seconds_elapsed);
         const auto speed = bytes_received / seconds_elapsed;
         const auto [converted_speed,speed_postfix] = stringify_bytes(static_cast<double>(speed),Conversion_Format::Speed);

         download_speed_label_.setText(QString("%1 %2").arg(converted_speed).arg(speed_postfix.data()));
}