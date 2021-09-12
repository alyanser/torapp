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
         Q_OBJECT
public:
         enum class Misc_State { No_State, File_Write_Error, Unknown_Network_Error, Download_Finished, Custom_State };

         Download_status_tracker(const QString & package_name,const QString & download_path);

         void bind_lifetime_with_cancel_button() noexcept;
private:
         void setup_file_state_layout() noexcept;
         void setup_network_state_layout() noexcept;
         void setup_state_widget() noexcept;
         void update_state_line() noexcept;

         Misc_State misc_state_ = Misc_State::No_State;

         QVBoxLayout central_layout_ = QVBoxLayout(this);
         QHBoxLayout file_stat_layout_;
         QHBoxLayout network_stat_layout_;
         //todo add buddies
         QLabel package_name_buddy_ = QLabel("Name: ");
         QLabel download_path_buddy_ = QLabel("Path: ");
         QLabel package_name_label_;
         QLabel download_path_label_;

         QStackedWidget state_widget_;

         QLineEdit state_line_;
         QProgressBar download_progress_bar_;

         QPushButton cancel_button_ = QPushButton("Cancel");

signals:
         void request_cancelled() const;

public slots:
         void set_misc_state(Misc_State new_misc_state) noexcept;
         void set_custom_state(const QString & custom_state) noexcept;
};

inline Download_status_tracker::Download_status_tracker(const QString & package_name,const QString & download_path){
         Expects(!package_name.isEmpty());
         Expects(!download_path.isEmpty());

         package_name_label_.setText(package_name);
         download_path_label_.setText(download_path);

         central_layout_.addLayout(&file_stat_layout_);
         central_layout_.addWidget(&state_widget_);
         central_layout_.addLayout(&network_stat_layout_);

         setup_file_state_layout();
         setup_state_widget();
         setup_network_state_layout();
}

inline void Download_status_tracker::bind_lifetime_with_cancel_button() noexcept {

         //! potential bug
         connect(&cancel_button_,&QPushButton::clicked,this,[self = shared_from_this()]{
                  emit self->request_cancelled();
                  Ensures(self.unique());

         },Qt::SingleShotConnection);
}

inline void Download_status_tracker::setup_file_state_layout() noexcept {
         file_stat_layout_.addWidget(&package_name_label_);
         file_stat_layout_.addWidget(&download_path_label_);
}

inline void Download_status_tracker::setup_network_state_layout() noexcept {
         network_stat_layout_.addWidget(&cancel_button_);
}

inline void Download_status_tracker::setup_state_widget() noexcept {
         state_widget_.addWidget(&download_progress_bar_);
         state_widget_.addWidget(&state_line_);

         Ensures(!state_widget_.currentIndex());
}

inline void Download_status_tracker::set_misc_state(const Misc_State new_misc_state) noexcept {
         Expects(new_misc_state != Misc_State::No_State && new_misc_state != Misc_State::Custom_State);

         misc_state_ = new_misc_state;
         update_state_line();
}

inline void Download_status_tracker::set_custom_state(const QString & custom_state) noexcept {
         misc_state_ = Misc_State::Custom_State;
         state_line_.setText(custom_state);
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