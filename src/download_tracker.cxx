#include "download_tracker.hxx"

#include <bencode_parser.hxx>
#include <QDesktopServices>
#include <QMessageBox>
#include <QDir>

Download_tracker::Download_tracker(const QString & dl_path,const Download_Type dl_type,QWidget * const parent) 
         : QWidget(parent), dl_path_(dl_path),dl_type_(dl_type)
{
         setMaximumHeight(200);
         setup_layout();
         setup_file_status_layout();
         setup_network_status_layout();
         setup_state_stack();
         update_error_line();
         configure_default_connections();

         open_button_.setEnabled(false);
         delete_button_.setEnabled(false);
         dl_progress_bar_.setTextVisible(true);
         dl_path_label_.setText(dl_path_);

         read_settings();

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
                           constexpr std::string_view message_body("Downloaded file (s) could not be opened");
                           QMessageBox::critical(this,message_title.data(),message_body.data());
                  }
         });
}

Download_tracker::Download_tracker(const QString & dl_path,const QUrl url,QWidget * const parent) 
         : Download_tracker(dl_path,Download_Type::Url,parent)
{
         assert(!url.isEmpty());
         assert(!dl_path.isEmpty());
         
         package_name_label_.setText(url.fileName());

         connect(&retry_button_,&QPushButton::clicked,this,[this,dl_path,url]{
                  emit retry_download(dl_path,url);
                  emit request_satisfied();
         });
}

Download_tracker::Download_tracker(const QString & dl_path,bencode::Metadata torrent_metadata,QWidget * const parent) 
         : Download_tracker(dl_path,Download_Type::Torrent,parent)
{
         package_name_label_.setText(torrent_metadata.name.data());

         connect(&retry_button_,&QPushButton::clicked,this,[this,dl_path,torrent_metadata = std::move(torrent_metadata)]{
                  emit retry_download(dl_path,torrent_metadata);
                  emit request_satisfied();
         });
}

void Download_tracker::setup_file_status_layout() noexcept {
         file_stat_layout_.addLayout(&package_name_layout_);
         file_stat_layout_.addLayout(&dl_path_layout_);
         file_stat_layout_.addLayout(&time_elapsed_layout_);

         package_name_layout_.addWidget(&package_name_buddy_);
         package_name_layout_.addWidget(&package_name_label_);
         package_name_buddy_.setBuddy(&package_name_label_);

         dl_path_layout_.addWidget(&dl_path_buddy_);
         dl_path_layout_.addWidget(&dl_path_label_);
         dl_path_layout_.addWidget(&open_dir_button_);
         dl_path_buddy_.setBuddy(&dl_path_label_);

         time_elapsed_layout_.addWidget(&time_elapsed_buddy_);
         time_elapsed_layout_.addWidget(&time_elapsed_label_);
         time_elapsed_buddy_.setBuddy(&time_elapsed_label_);
}

void Download_tracker::setup_network_status_layout() noexcept {
         network_form_layout_.setFormAlignment(Qt::AlignCenter);
         network_form_layout_.setSpacing(15);

         network_stat_layout_.addLayout(&network_form_layout_);

         network_form_layout_.addRow("Download Speed",&dl_speed_label_);
         network_form_layout_.addRow("Downloaded",&dl_quantity_label_);
         network_form_layout_.addRow("Uploaded",&ul_quantity_label_);

         network_stat_layout_.addWidget(&delete_button_);
         network_stat_layout_.addWidget(&state_button_stack_);
         network_stat_layout_.addWidget(&initiate_button_stack_);
         network_stat_layout_.addWidget(&terminate_button_stack_);

         initiate_button_stack_.addWidget(&open_button_);
         initiate_button_stack_.addWidget(&retry_button_);

         terminate_button_stack_.addWidget(&cancel_button_);
         terminate_button_stack_.addWidget(&finish_button_);

         state_button_stack_.addWidget(&pause_button_);
         state_button_stack_.addWidget(&resume_button_);
}

void Download_tracker::download_progress_update(const std::int64_t received_byte_cnt,const std::int64_t total_byte_cnt) noexcept {
         dled_byte_cnt_ = received_byte_cnt;
         total_byte_cnt_ = total_byte_cnt;

         if(state_ == State::Verification){
                  restored_byte_cnt_ = dled_byte_cnt_;
         }

         assert(received_byte_cnt >= 0);
         assert(!dl_progress_bar_.minimum());

         if(constexpr auto unknown_byte_cnt = -1;total_byte_cnt == unknown_byte_cnt){
                  dl_progress_bar_.setRange(0,0); // sets in pending state
         }else{
                  // ! consider the overflow
                  dl_progress_bar_.setMaximum(static_cast<std::int32_t>(total_byte_cnt));
                  dl_progress_bar_.setValue(static_cast<std::int32_t>(received_byte_cnt));
         }

         const auto text_fmt = util::conversion::stringify_bytes(received_byte_cnt,total_byte_cnt);
         dl_quantity_label_.setText(text_fmt);

         using util::conversion::convert_to_percentile;
         dl_progress_bar_.setFormat(text_fmt + (total_byte_cnt < 1 ? " nan %" : " " + QString::number(convert_to_percentile(received_byte_cnt,total_byte_cnt)) + "%"));
}

void Download_tracker::set_state(const State state) noexcept {
         state_ = state;

         if(state_ == State::Download){
                  session_timer_.start(std::chrono::seconds(1));
                  state_stack_.setCurrentWidget(&dl_progress_bar_);
         }else{
                  assert(state_ == State::Verification);
                  state_stack_.setCurrentWidget(&verify_progress_bar_);
         }
}

void Download_tracker::update_download_speed() noexcept {
         assert(dled_byte_cnt_ >= restored_byte_cnt_);
         assert(session_time_.count() >= 0);
         const auto speed = session_time_.count() ? (dled_byte_cnt_ - restored_byte_cnt_) / session_time_.count() : 0;
         const auto [converted_speed,speed_postfix] = stringify_bytes(static_cast<double>(speed),util::conversion::Format::Speed);
         dl_speed_label_.setText(QString("%1 %2").arg(converted_speed).arg(speed_postfix.data()));
}

void Download_tracker::setup_state_stack() noexcept {
         state_stack_.addWidget(&dl_progress_bar_);
         state_stack_.addWidget(&verify_progress_bar_);
         state_stack_.addWidget(&finish_line_);

         dl_progress_bar_.setMinimum(0);
         dl_progress_bar_.setValue(0);
         
         finish_line_.setAlignment(Qt::AlignCenter);
         assert(state_stack_.currentWidget() == &dl_progress_bar_);
}

void Download_tracker::verification_progress_update(std::int32_t verified_asset_cnt,std::int32_t total_asset_cnt) noexcept {
         assert(total_asset_cnt);
         assert(state_ == State::Verification);
         verify_progress_bar_.setValue(verified_asset_cnt);
         verify_progress_bar_.setMaximum(total_asset_cnt);
         verify_progress_bar_.setFormat("Verifying " + QString::number(util::conversion::convert_to_percentile(verified_asset_cnt,total_asset_cnt)) + "%");
}

void Download_tracker::write_settings() const noexcept {
         QSettings settings;
         begin_setting_groups(settings);
         settings.setValue("time_elapsed",QVariant::fromValue(time_elapsed_));
}

void Download_tracker::read_settings() noexcept {
         QSettings settings;
         begin_setting_groups(settings);

         time_elapsed_ = qvariant_cast<QTime>(settings.value("time_elapsed"));

         if(!time_elapsed_.isValid()){
                  time_elapsed_ = QTime(0,0,0);
         }

         time_elapsed_label_.setText(time_elapsed_.toString() + time_elapsed_fmt.data());
}

void Download_tracker::configure_default_connections() noexcept {
         connect(&finish_button_,&QPushButton::clicked,this,&Download_tracker::download_dropped);
         connect(&finish_button_,&QPushButton::clicked,this,&Download_tracker::request_satisfied);
         connect(this,&Download_tracker::download_dropped,this,&Download_tracker::request_satisfied);
         connect(this,&Download_tracker::request_satisfied,&Download_tracker::deleteLater);

         connect(&pause_button_,&QPushButton::clicked,this,[this]{
                  assert(session_timer_.isActive());
                  session_timer_.stop();
                  state_button_stack_.setCurrentWidget(&resume_button_);

                  session_time_ = std::chrono::seconds::zero();
                  update_download_speed();
                  emit download_paused();
         });

         connect(&resume_button_,&QPushButton::clicked,this,[this]{
                  assert(!session_timer_.isActive());
                  session_timer_.start();
                  state_button_stack_.setCurrentWidget(&pause_button_);
                  emit download_resumed();
         });

         connect(&delete_button_,&QPushButton::clicked,this,[this]{
                  QMessageBox query_box(QMessageBox::Icon::Warning,"Delete file (s)","Choose an action",QMessageBox::Button::NoButton);

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
                  
                  const auto reply_button = QMessageBox::question(this,question_title.data(),question_body.data(),buttons);

                  if(reply_button == QMessageBox::StandardButton::Yes){
                           session_timer_.stop();
                           emit download_dropped();
                  }
         });

         session_timer_.callOnTimeout(this,[this]{
                  time_elapsed_ = time_elapsed_.addSecs(1);
                  time_elapsed_label_.setText(time_elapsed_.toString() + time_elapsed_fmt.data());
                  ++session_time_;
                  update_download_speed();

                  QTimer::singleShot(0,this,[this]{
                           write_settings();
                  });
         });
}

void Download_tracker::switch_to_finished_state() noexcept {
         time_elapsed_buddy_.setText("Time took: ");
         state_stack_.setCurrentWidget(&finish_line_);
         terminate_button_stack_.setCurrentWidget(&finish_button_);
         
         if(error_ == Error::Null){
                  initiate_button_stack_.setCurrentWidget(&retry_button_);
         }else{
                  assert(!delete_button_.isEnabled());
                  assert(!open_button_.isEnabled());
                  delete_button_.setEnabled(true);
                  open_button_.setEnabled(true);
         }
}

void Download_tracker::update_error_line() noexcept {

         switch(error_){
                  
                  case Error::Null : {
                           finish_line_.setText("Download completed successfully. Click on open button to view");
                           break;
                  }

                  case Error::File_Write : {
                           finish_line_.setText("Given file could not be opened for writing");
                           break;
                  }

                  case Error::Unknown_Network : {
                           finish_line_.setText("Unknown network error. Try restarting the download");
                           break;
                  }
                  
                  case Error::File_Lock : { 
                           finish_line_.setText("Given path already exists. Cannot overwire");
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