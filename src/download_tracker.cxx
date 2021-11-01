#include "download_tracker.hxx"

#include <bencode_parser.hxx>
#include <QDesktopServices>
#include <QMessageBox>
#include <QDir>

Download_tracker::Download_tracker(const QString & dl_path,const Download_Type dl_type,QWidget * const parent) 
         : QFrame(parent), dl_path_(dl_path),dl_type_(dl_type)
{
         setFixedHeight(230); // todo: figure later

         setFrameShadow(QFrame::Shadow::Sunken);
         setFrameShape(QFrame::Shape::Box);
         setLineWidth(3);

         setup_layout();
         setup_file_status_layout();
         setup_network_status_layout();
         setup_state_stack();
         configure_default_connections();
         read_settings();

         central_layout_.setSpacing(15);

         open_button_.setEnabled(false);
         delete_button_.setEnabled(false);
         properties_button_.setEnabled(false);
         pause_button_.setEnabled(false);

         dl_progress_bar_.setTextVisible(true);
         dl_progress_bar_.setRange(0,0);

         package_name_label_.setFrameShadow(QFrame::Shadow::Raised);
         package_name_label_.setFrameShape(QFrame::Shape::Panel);
         package_name_label_.setAlignment(Qt::AlignCenter);

         time_elapsed_label_.setFrameShadow(QFrame::Shadow::Raised);
         time_elapsed_label_.setFrameShape(QFrame::Shape::Panel);
         time_elapsed_label_.setAlignment(Qt::AlignCenter);

         connect(&open_dir_button_,&QPushButton::clicked,this,[this,dl_path]{

                  if(QFileInfo file_info(dl_path);!QDesktopServices::openUrl(file_info.absolutePath())){
                           constexpr std::string_view error_title("Directory open error");
                           constexpr std::string_view error_body("Directory could not be opened");
                           QMessageBox::critical(this,error_title.data(),error_body.data());
                  }
         });

         connect(&open_button_,&QPushButton::clicked,this,[this,dl_type,dl_path]{
                  
                  if(QFileInfo file_info(dl_path);!QDesktopServices::openUrl(dl_type == Download_Type::Torrent ? file_info.absolutePath() : file_info.absoluteFilePath())){
                           constexpr std::string_view message_title("Could not open file");
                           constexpr std::string_view message_body("Downloaded file (s) could not be opened");
                           QMessageBox::critical(this,message_title.data(),message_body.data());
                  }else{
                           qDebug() << dl_path << "could not be opened";
                  }
         });
}

Download_tracker::Download_tracker(const QString & dl_path,const QUrl url,QWidget * const parent) 
         : Download_tracker(dl_path,Download_Type::Url,parent)
{
         assert(!url.isEmpty());
         assert(!dl_path.isEmpty());

         package_name_label_.setText(QUrl(dl_path).fileName());

         auto restart_download = [this,dl_path,url]{
                  emit retry_download(dl_path,url);
                  emit request_satisfied();
         };

         connect(&retry_button_,&QPushButton::clicked,this,restart_download);
         connect(&resume_button_,&QPushButton::clicked,this,restart_download);
}

Download_tracker::Download_tracker(const QString & dl_path,bencode::Metadata torrent_metadata,QWidget * const parent) 
         : Download_tracker(dl_path,Download_Type::Torrent,parent)
{
         package_name_label_.setText(torrent_metadata.name.empty() ? "N/A" : torrent_metadata.name.data());

         connect(&retry_button_,&QPushButton::clicked,this,[this,dl_path,torrent_metadata = std::move(torrent_metadata)]{
                  emit retry_download(dl_path,torrent_metadata);
                  emit request_satisfied();
         });
}

void Download_tracker::setup_file_status_layout() noexcept {
         file_stat_layout_.addLayout(&package_name_layout_);
         file_stat_layout_.addLayout(&time_elapsed_layout_);
         file_stat_layout_.addWidget(&open_dir_button_);
         file_stat_layout_.addWidget(&properties_button_);

         package_name_layout_.addWidget(&package_name_buddy_);
         package_name_layout_.addWidget(&package_name_label_);
         package_name_buddy_.setBuddy(&package_name_label_);

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

         if(dl_type_ == Download_Type::Torrent){
                  network_form_layout_.addRow("Uploaded",&ul_quantity_label_);
                  network_form_layout_.addRow("Ratio",&ratio_label_);
         }

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

void Download_tracker::set_error_and_finish(const Error error) noexcept {
         update_finish_line(error);
         switch_to_finished_state(error);
}

void Download_tracker::set_error_and_finish(const QString & error_desc) noexcept {
         assert(!error_desc.isEmpty());
         finish_line_.setText(error_desc);
         switch_to_finished_state(Error::Custom);
}

void Download_tracker::download_progress_update(const std::int64_t received_byte_cnt,const std::int64_t total_byte_cnt) noexcept {
         assert(received_byte_cnt >= 0);
         assert(!dl_progress_bar_.minimum());

         {
                  const auto newly_dled_byte_cnt = received_byte_cnt - dled_byte_cnt_;
                  assert(newly_dled_byte_cnt >= 0);

                  dled_byte_cnt_ = received_byte_cnt;
                  total_byte_cnt_ = total_byte_cnt;

                  state_ == State::Verification ? restored_byte_cnt_ = dled_byte_cnt_ : session_dled_byte_cnt_ += newly_dled_byte_cnt;
         }
         
         if(constexpr auto unknown_byte_cnt = -1;total_byte_cnt == unknown_byte_cnt){
                  dl_progress_bar_.setRange(0,0); // sets in pending state
         }else{
                  dl_progress_bar_.setValue(static_cast<std::int32_t>(util::conversion::stringify_bytes(received_byte_cnt,util::conversion::Format::Memory).first));
                  dl_progress_bar_.setMaximum(static_cast<std::int32_t>(util::conversion::stringify_bytes(total_byte_cnt,util::conversion::Format::Memory).first));
         }

         const auto text_fmt = util::conversion::stringify_bytes(received_byte_cnt,total_byte_cnt);
         dl_quantity_label_.setText(text_fmt);

         dl_progress_bar_.setFormat(text_fmt + (total_byte_cnt < 1 ? " nan %" : ' ' + util::conversion::convert_to_percent_format(received_byte_cnt,total_byte_cnt)));
}

void Download_tracker::set_state(const State state) noexcept {
         state_ = state;

         if(state_ == State::Download){
                  pause_button_.setEnabled(true);
                  session_timer_.start(std::chrono::seconds(1));
                  progress_bar_stack_.setCurrentWidget(&dl_progress_bar_);
         }else{
                  assert(dl_type_ == Download_Type::Torrent);
                  assert(state_ == State::Verification);
                  progress_bar_stack_.setCurrentWidget(&verify_progress_bar_);
         }
}

void Download_tracker::update_download_speed() noexcept {
         assert(session_time_.count() >= 0);
         assert(session_dled_byte_cnt_ >= 0);
         const auto speed = session_time_.count() ? session_dled_byte_cnt_ / session_time_.count() : 0;
         const auto [converted_speed,speed_postfix] = stringify_bytes(static_cast<double>(speed),util::conversion::Format::Speed);
         dl_speed_label_.setText(QString("%1 %2").arg(converted_speed).arg(speed_postfix.data()));
}

void Download_tracker::setup_state_stack() noexcept {
         progress_bar_stack_.addWidget(&dl_progress_bar_);
         progress_bar_stack_.addWidget(&verify_progress_bar_);
         progress_bar_stack_.addWidget(&finish_line_);

         dl_progress_bar_.setMinimum(0);
         dl_progress_bar_.setValue(0);
         
         finish_line_.setAlignment(Qt::AlignCenter);
         assert(progress_bar_stack_.currentWidget() == &dl_progress_bar_);
}

void Download_tracker::verification_progress_update(std::int32_t verified_asset_cnt,std::int32_t total_asset_cnt) noexcept {
         assert(total_asset_cnt);
         assert(state_ == State::Verification);
         verify_progress_bar_.setValue(verified_asset_cnt);
         verify_progress_bar_.setMaximum(total_asset_cnt);
         verify_progress_bar_.setFormat("Verifying " + util::conversion::convert_to_percent_format(verified_asset_cnt,total_asset_cnt));
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

         if(dl_type_ == Download_Type::Torrent){
                  connect(&properties_button_,&QPushButton::clicked,this,&Download_tracker::properties_button_clicked);
         }

         connect(&pause_button_,&QPushButton::clicked,this,[this]{
                  state_button_stack_.setCurrentWidget(&resume_button_);
                  assert(session_timer_.isActive());
                  session_timer_.stop();

                  if(dl_type_ == Download_Type::Url){
                           emit download_stopped();
                  }else{
                           session_dled_byte_cnt_ = 0;
                           session_time_ = std::chrono::seconds::zero();
                           update_download_speed();
                           emit download_paused();
                  }
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

                  connect(delete_permanently_button,&QPushButton::clicked,this,&Download_tracker::delete_files_permanently);
                  connect(move_to_trash_button,&QPushButton::clicked,this,&Download_tracker::move_files_to_trash);
                  connect(this,&Download_tracker::delete_files_permanently,this,&Download_tracker::download_dropped);
                  connect(this,&Download_tracker::move_files_to_trash,this,&Download_tracker::download_dropped);

                  query_box.exec();
         });

         connect(&cancel_button_,&QPushButton::clicked,this,[this]{
                  constexpr std::string_view query_title("Cancel Download");
                  constexpr std::string_view query_body("Are you sure you want to cancel the download?");
                  constexpr auto buttons = QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No;
                  
                  const auto reply_button = QMessageBox::question(this,query_title.data(),query_body.data(),buttons);

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
                  write_settings();
         });
}

void Download_tracker::on_verification_completed() noexcept {
         assert(!properties_button_.isEnabled());
         properties_button_.setEnabled(true);
         assert(dled_byte_cnt_ >= 0 && dled_byte_cnt_ <= total_byte_cnt_);
         pause_button_.setEnabled(dled_byte_cnt_ != total_byte_cnt_);
}

void Download_tracker::switch_to_finished_state(const Error error) noexcept {
         time_elapsed_buddy_.setText("Time took: ");
         progress_bar_stack_.setCurrentWidget(&finish_line_);
         terminate_button_stack_.setCurrentWidget(&finish_button_);
         pause_button_.setEnabled(false);
         resume_button_.setEnabled(false);
         session_timer_.stop();
         dl_speed_label_.hide();
         
         if(error == Error::Null){
                  assert(initiate_button_stack_.currentWidget() == &open_button_);
                  assert(!delete_button_.isEnabled());
                  assert(!open_button_.isEnabled());
                  delete_button_.setEnabled(true);
                  open_button_.setEnabled(true);
         }else{
                  initiate_button_stack_.setCurrentWidget(&retry_button_);
         }
}

void Download_tracker::update_finish_line(const Error error) noexcept {
         assert(error != Error::Custom);

         finish_line_.setText([error]{
                  // ? consider using static QStrings or QStrlingLiteral
                  switch(error){
                           
                           case Error::Null : {
                                    return "Download finished";
                           }

                           case Error::File_Write : {
                                    return "Given path could not be opened for writing";
                           }

                           case Error::Network : {
                                    return "Unknown network error";
                           }
                           
                           case Error::File_Lock : { 
                                    return "Given path is already locked by another process";
                           }

                           case Error::Space : {
                                    return "Not enough space";
                           }

                           default : {
                                    return "Oops. Something went wrong";
                           }
                  }
         }());
}