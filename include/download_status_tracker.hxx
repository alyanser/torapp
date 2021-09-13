#ifndef DOWNLOAD_STATUS_TRACKER_HXX
#define DOWNLOAD_STATUS_TRACKER_HXX

#include <QString>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QProgressBar>
#include <QStackedWidget>
#include <utility>
#include <QMessageBox>

class Download_status_tracker : public QWidget, public std::enable_shared_from_this<Download_status_tracker> {
         Q_OBJECT
public:
         enum class Misc_State { No_State, File_Write_Error, Unknown_Network_Error, Download_Finished, Custom_State };

         Download_status_tracker(const QString & package_name,const QString & download_path);

         void bind_lifetime_with_close_button() noexcept;
private:
         void setup_file_status_layout() noexcept;
         void setup_network_status_layout() noexcept;
         void setup_state_widget() noexcept;
         void update_state_line() noexcept;

         Misc_State misc_state_ = Misc_State::No_State;

         QVBoxLayout central_layout_ = QVBoxLayout(this);
         QHBoxLayout file_stat_layout_;
         QHBoxLayout network_stat_layout_;

         QHBoxLayout package_name_layout_;
         QLabel package_name_buddy_ = QLabel("Name: ");
         QLabel package_name_label_;

         QHBoxLayout download_path_layout_;
         QLabel download_path_buddy_ = QLabel("Path: ");
         QLabel download_path_label_;

         QStackedWidget state_widget_;

         QLineEdit state_line_;
         QProgressBar download_progress_bar_;

         QHBoxLayout download_quantity_layout_;
         QLabel download_quantity_buddy_ = QLabel("Downloaded: ");
         QLabel download_quantity_line_ = QLabel("0/0 kb");

         QHBoxLayout upload_quantity_layout_;
         QLabel upload_quantity_buddy_ = QLabel("Uploaded: ");
         QLabel upload_quantity_line_ = QLabel("0/0 kb");

         QPushButton close_button_ = QPushButton("Cancel");

signals:
         void request_satisfied() const;

public slots:
         void set_misc_state(Misc_State new_misc_state) noexcept;
         void set_custom_state(const QString & custom_state) noexcept;
         void on_download_finished() noexcept;
         void download_progress_update(int64_t bytes_received,int64_t total_bytes) noexcept;
         void upload_progress_update(int64_t bytes_sent,int64_t total_bytes) noexcept;
};

inline Download_status_tracker::Download_status_tracker(const QString & package_name,const QString & download_path){
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
}

inline void Download_status_tracker::setup_file_status_layout() noexcept {
         file_stat_layout_.addLayout(&package_name_layout_);
         file_stat_layout_.addLayout(&download_path_layout_);

         package_name_layout_.addWidget(&package_name_buddy_);
         package_name_layout_.addWidget(&package_name_label_);
         package_name_buddy_.setBuddy(&package_name_label_);

         download_path_layout_.addWidget(&download_path_buddy_);
         download_path_layout_.addWidget(&download_path_label_);
         download_path_buddy_.setBuddy(&download_path_label_);
}

inline void Download_status_tracker::setup_network_status_layout() noexcept {
         network_stat_layout_.addLayout(&download_quantity_layout_);
         network_stat_layout_.addLayout(&upload_quantity_layout_);

         download_quantity_buddy_.setBuddy(&download_quantity_line_);
         download_quantity_layout_.addWidget(&download_quantity_buddy_);
         download_quantity_layout_.addWidget(&download_quantity_line_);

         upload_quantity_buddy_.setBuddy(&upload_quantity_line_);
         upload_quantity_layout_.addWidget(&upload_quantity_buddy_);
         upload_quantity_layout_.addWidget(&upload_quantity_line_);
         
         network_stat_layout_.addWidget(&close_button_);
}

inline void Download_status_tracker::setup_state_widget() noexcept {
         state_widget_.addWidget(&download_progress_bar_);
         state_widget_.addWidget(&state_line_);

         download_progress_bar_.setMinimum(0);
         download_progress_bar_.setMinimum(100);

         assert(!state_widget_.currentIndex());
}

inline void Download_status_tracker::set_misc_state(const Misc_State new_misc_state) noexcept {
         assert(new_misc_state != Misc_State::No_State);
         assert(new_misc_state != Misc_State::Custom_State);

         misc_state_ = new_misc_state;
         update_state_line();

         state_widget_.setCurrentWidget(&state_line_);
}

inline void Download_status_tracker::set_custom_state(const QString & custom_state) noexcept {
         misc_state_ = Misc_State::Custom_State;
         state_line_.setText(custom_state);

         state_widget_.setCurrentWidget(&state_line_);
}

inline void Download_status_tracker::on_download_finished() noexcept {
         close_button_.setText("Finish");
}

inline void Download_status_tracker::download_progress_update(const int64_t bytes_received,const int64_t total_bytes) noexcept {
         constexpr int32_t unknown_size = -1;

         if(total_bytes == unknown_size){
                  download_quantity_line_.setText(QString("%1/%2 kb").arg(bytes_received / 1000).arg("inf"));
                  // set the bar in waiting state
                  download_progress_bar_.setMinimum(0);
                  download_progress_bar_.setMaximum(0);
         }else if(total_bytes){
                  assert(total_bytes > 0);
                  download_quantity_line_.setText(QString("%1/%2 kb").arg(bytes_received / 1000).arg(total_bytes / 1000));
                  
                  download_progress_bar_.setMaximum(total_bytes);
                  download_progress_bar_.setValue(bytes_received);
         }else{
                  // download could not be initiated
                  //? how to react
         }
}

inline void Download_status_tracker::upload_progress_update(const int64_t bytes_sent,const int64_t total_bytes) noexcept {
         upload_quantity_line_.setText(QString("%1/%2 kb").arg(bytes_sent / 1000).arg(total_bytes / 1000));
}

inline void Download_status_tracker::bind_lifetime_with_close_button() noexcept {

         connect(this,&Download_status_tracker::request_satisfied,this,[self = shared_from_this()]{
                  //! potential bug
                  assert(self.unique());

         },Qt::SingleShotConnection);

         connect(&close_button_,&QPushButton::clicked,this,[this]{

                  if(close_button_.text() == "Cancel"){
                           constexpr std::string_view question_title("Cancel Download");
                           constexpr std::string_view question_body("Are you sure you want to cancel the download?");

                           constexpr auto buttons = QMessageBox::Yes | QMessageBox::No;
                           
                           const auto response = QMessageBox::question(this,question_title.data(),question_body.data(),buttons);

                           if(response == QMessageBox::No){
                                    return;
                           }
                  }
                  
                  emit request_satisfied();
         });
}

inline void Download_status_tracker::update_state_line() noexcept {
         constexpr std::string_view no_state_info("You should not be seeing this right now. Something seriously has gone wrong");
         constexpr std::string_view file_write_error_info("File could not be opened for writing");
         constexpr std::string_view unknown_network_error_info("Unknown network error. Try restarting the download");
         constexpr std::string_view download_finished_info("Download has been finished");
         
         switch(misc_state_){
                  case Misc_State::No_State : state_line_.setText(no_state_info.data()); break;
                  case Misc_State::File_Write_Error : state_line_.setText(file_write_error_info.data()); break;
                  case Misc_State::Unknown_Network_Error : state_line_.setText(unknown_network_error_info.data()); break;
                  case Misc_State::Download_Finished : state_line_.setText(download_finished_info.data()); break;
                  case Misc_State::Custom_State : [[fallthrough]];
                  default : __builtin_unreachable();
         }
}

#endif // STATUS_TRACKER_HXX