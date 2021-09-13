#include "download_status_tracker.hxx"

Download_status_tracker::Download_status_tracker(const QString & package_name,const QString & download_path){
         assert(!package_name.isEmpty());
         assert(!download_path.isEmpty());

         package_name_label_.setText(package_name);
         download_path_label_.setText(download_path);

         central_layout_.addLayout(&file_stat_layout_);
         central_layout_.addWidget(&state_widget_);
         central_layout_.addLayout(&network_stat_layout_);

         setup_file_status_layout();
         setup_state_widget();
         setup_network_status_layout();

         {
                  using namespace std::chrono_literals;

                  time_elapsed_timer_.setInterval(1000ms);
                  //? what is start
                  time_elapsed_timer_.start(1000ms);

                  connect(&time_elapsed_timer_,&QTimer::timeout,&time_elapsed_label_,[this](){
                           time_elapsed_ = time_elapsed_.addSecs(1);
                           time_elapsed_label_.setText(time_elapsed_.toString() + " hh:mm::ss");
                  });
         }

         connect(&open_button_,&QPushButton::clicked,[this,download_path]{
                  
                  //! if doesn't exist then creates new one
                  if(!QDesktopServices::openUrl(QUrl(download_path))){
                           constexpr std::string_view message_title("Could not open file");
                           constexpr std::string_view message_body("Downloaded file could not be opened");

                           QMessageBox::warning(this,message_title.data(),message_body.data());
                  }
         });
}

void Download_status_tracker::setup_file_status_layout() noexcept {
         file_stat_layout_.addLayout(&package_name_layout_);
         file_stat_layout_.addLayout(&download_path_layout_);
         file_stat_layout_.addLayout(&time_elapsed_layout_);

         package_name_layout_.addWidget(&package_name_buddy_);
         package_name_layout_.addWidget(&package_name_label_);
         package_name_buddy_.setBuddy(&package_name_label_);

         download_path_layout_.addWidget(&download_path_buddy_);
         download_path_layout_.addWidget(&download_path_label_);
         download_path_buddy_.setBuddy(&download_path_label_);

         time_elapsed_layout_.addWidget(&time_elapsed_buddy_);
         time_elapsed_layout_.addWidget(&time_elapsed_label_);
         time_elapsed_buddy_.setBuddy(&time_elapsed_label_);
}

void Download_status_tracker::setup_network_status_layout() noexcept {
         network_stat_layout_.addLayout(&download_speed_layout_);
         network_stat_layout_.addLayout(&download_quantity_layout_);
         network_stat_layout_.addLayout(&upload_quantity_layout_);

         download_speed_layout_.addWidget(&download_speed_buddy_);
         download_speed_layout_.addWidget(&download_speed_label_);
         download_speed_buddy_.setBuddy(&download_speed_label_);

         download_quantity_buddy_.setBuddy(&download_quantity_label_);
         download_quantity_layout_.addWidget(&download_quantity_buddy_);
         download_quantity_layout_.addWidget(&download_quantity_label_);

         upload_quantity_buddy_.setBuddy(&upload_quantity_label_);
         upload_quantity_layout_.addWidget(&upload_quantity_buddy_);
         upload_quantity_layout_.addWidget(&upload_quantity_label_);
         
         network_stat_layout_.addWidget(&open_button_);
         network_stat_layout_.addWidget(&close_button_);

         open_button_.setEnabled(false);
}

void Download_status_tracker::download_progress_update(const int64_t bytes_received,const int64_t total_bytes) noexcept {

         if(!total_bytes){ // download couldn't initiate
                  return;
         }

         const auto seconds_elapsed = time_elapsed_.second() + time_elapsed_.minute() * 60 + time_elapsed_.hour() * 3600;

         //todo bytes | kbs | mbs | gbs
}