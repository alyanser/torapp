#ifndef DOWNLOAD_STATUS_TRACKER_HXX
#define DOWNLOAD_STATUS_TRACKER_HXX

#include <gsl/assert>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QProgressBar>
#include <QStackedWidget>
#include <utility>

class Download_status_tracker : public QWidget, public std::enable_shared_from_this<Download_status_tracker> {
public:
         enum class State { No_State, File_Write_Error, Unknown_Network_Error, Download_Finished };

         Download_status_tracker(const QString & package_name,const QString & download_path);

         void bind_lifetime_with_cancel_button() noexcept;
         void set_state(State new_state) noexcept;
private:
         void setup_file_stat_layout() noexcept;
         void setup_network_stat_layout() noexcept;
         void setup_state_widget() noexcept;
         void update_state_line() noexcept;

         State current_state_ = State::No_State;
         QVBoxLayout central_layout_ = QVBoxLayout(this);
         QHBoxLayout file_stat_layout_;
         QHBoxLayout network_stat_layout_;

         QLabel package_name_label_;
         QLabel download_path_label_;

         QStackedWidget state_widget_;

         QLineEdit error_line_;
         QProgressBar download_progress_bar_;

         QPushButton cancel_button_ = QPushButton("Cancel");
};

inline Download_status_tracker::Download_status_tracker(const QString & package_name,const QString & download_path){
         Expects(!package_name.isEmpty());
         Expects(!download_path.isEmpty());

         package_name_label_.setText(package_name);
         download_path_label_.setText(download_path);

         central_layout_.addLayout(&file_stat_layout_);
         central_layout_.addWidget(&state_widget_);
         central_layout_.addLayout(&network_stat_layout_);

         setup_file_stat_layout();
         setup_state_widget();
         setup_network_stat_layout();
}

inline void Download_status_tracker::bind_lifetime_with_cancel_button() noexcept {

         connect(&cancel_button_,&QPushButton::clicked,this,[self = shared_from_this()](){
                  Ensures(self.unique());

         },Qt::SingleShotConnection);
}

inline void Download_status_tracker::setup_file_stat_layout() noexcept {
         file_stat_layout_.addWidget(&package_name_label_);
         file_stat_layout_.addWidget(&download_path_label_);
}

inline void Download_status_tracker::setup_network_stat_layout() noexcept {
         network_stat_layout_.addWidget(&cancel_button_);
}

inline void Download_status_tracker::setup_state_widget() noexcept {
         state_widget_.addWidget(&download_progress_bar_);
         state_widget_.addWidget(&error_line_);
}

inline void Download_status_tracker::set_state(const State new_state) noexcept {
         current_state_ = new_state;
         update_state_line();
}

inline void Download_status_tracker::update_state_line() noexcept {
         constexpr std::string_view file_write_error_info("File could not be opened for writing");
         constexpr std::string_view unknown_network_error_info("Unknown network error. Try restarting the download");
         constexpr std::string_view download_finished_info("Download has been finished");
         
         switch(current_state_){
                  case State::File_Write_Error : error_line_.setText(file_write_error_info.data()); break;
                  case State::Unknown_Network_Error : error_line_.setText(unknown_network_error_info.data()); break;
                  case State::Download_Finished : error_line_.setText(download_finished_info.data()); break;
                  default : __builtin_unreachable();
         }
}

#endif // STATUS_TRACKER_HXX