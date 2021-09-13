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
#include <QTimer>
#include <QTime>
#include <QDesktopServices>

class Download_status_tracker : public QWidget, public std::enable_shared_from_this<Download_status_tracker> {
         Q_OBJECT
public:
         enum class Conversion_Format { Speed, Memory };
         enum class Error { Null, File_Write, Unknown_Network, Custom };

         Download_status_tracker(const QUrl & package_url,const QString & download_path);

         [[nodiscard]] 
         static std::pair<double,std::string_view> stringify_bytes(double bytes,Conversion_Format format) noexcept;
         [[nodiscard]] 
         static QString stringify_bytes(int64_t updown_bytes_received,int64_t total_updown_bytes) noexcept;
         [[nodiscard]] 
         uint32_t get_elapsed_seconds() const noexcept;
         
         void bind_lifetime_with_terminate_holder() noexcept;

private:
         void setup_file_status_layout() noexcept;
         void setup_network_status_layout() noexcept;
         void setup_state_widget() noexcept;
         void update_state_line() noexcept;

         Error error_ = Error::Null;

         QVBoxLayout central_layout_ = QVBoxLayout(this);
         QHBoxLayout file_stat_layout_;
         QHBoxLayout network_stat_layout_;

         QHBoxLayout package_name_layout_;
         QLabel package_name_buddy_ = QLabel("Name: ");
         QLabel package_name_label_;

         QHBoxLayout download_path_layout_;
         QLabel download_path_buddy_ = QLabel("Path: ");
         QLabel download_path_label_;

         QStackedWidget state_holder_;
         QLineEdit state_line_;
         QProgressBar download_progress_bar_;

         QHBoxLayout download_quantity_layout_;
         QLabel download_quantity_buddy_ = QLabel("Downloaded: ");
         QLabel download_quantity_label_ = QLabel("0 byte(s) / 0 byte(s)");

         QHBoxLayout upload_quantity_layout_;
         QLabel upload_quantity_buddy_ = QLabel("Uploaded: ");
         QLabel upload_quantity_label_ = QLabel("0 byte(s) / 0 byte(s)");

         QStackedWidget terminate_buttons_holder_;
         QPushButton finish_button_ = QPushButton("Finish");
         QPushButton cancel_button_ = QPushButton("Cancel");

         QStackedWidget initiate_buttons_holder_;
         QPushButton open_button_ = QPushButton("Open");
         QPushButton retry_button_ = QPushButton("Retry");

         QHBoxLayout time_elapsed_layout_;
         QTime time_elapsed_ = QTime(0,0,1); // 1 to prevent division by zero
         QTimer time_elapsed_timer_;
         QLabel time_elapsed_buddy_ = QLabel("Time elapsed: ");
         QLabel time_elapsed_label_ = QLabel(time_elapsed_.toString() + " hh::mm::ss");

         QHBoxLayout download_speed_layout_;
         QLabel download_speed_buddy_ = QLabel("Download Speed: ");
         QLabel download_speed_label_ = QLabel("0 bytes/sec");

signals:
         void request_satisfied() const;
         void release_lifetime_from_terminate_holder() const;
         void retry_download(const QUrl & package_url,const QString & download_path) const;

public slots:
         void set_error(Error new_error) noexcept;
         void set_error(const QString & custom_error) noexcept;
         void on_download_finished() noexcept;
         void download_progress_update(int64_t bytes_received,int64_t total_bytes) noexcept;
         void upload_progress_update(int64_t bytes_sent,int64_t total_bytes) noexcept;
};

inline void Download_status_tracker::setup_state_widget() noexcept {
         state_holder_.addWidget(&download_progress_bar_);
         state_holder_.addWidget(&state_line_);

         download_progress_bar_.setMinimum(0);
         download_progress_bar_.setValue(0);
         state_line_.setAlignment(Qt::AlignCenter);

         assert(state_holder_.currentWidget() == &download_progress_bar_);
}

inline void Download_status_tracker::set_error(const Error new_error) noexcept {
         assert(new_error != Error::Null);
         assert(new_error != Error::Custom);

         error_ = new_error;
         update_state_line();

         state_holder_.setCurrentWidget(&state_line_);
}

inline void Download_status_tracker::set_error(const QString & custom_error) noexcept {
         error_ = Error::Custom;
         state_line_.setText(custom_error);

         state_holder_.setCurrentWidget(&state_line_);
}

inline void Download_status_tracker::on_download_finished() noexcept {

         time_elapsed_buddy_.setText("Time took: ");
         terminate_buttons_holder_.setCurrentWidget(&finish_button_);
         time_elapsed_timer_.stop();
         
         if(error_ == Error::Null){
                  open_button_.setEnabled(true);
         }else{
                  initiate_buttons_holder_.setCurrentWidget(&retry_button_);
         }
}

inline void Download_status_tracker::upload_progress_update(const int64_t bytes_sent,const int64_t total_bytes) noexcept {
         upload_quantity_label_.setText(stringify_bytes(bytes_sent,total_bytes));
}

inline std::pair<double,std::string_view> Download_status_tracker::stringify_bytes(const double bytes,const Conversion_Format format) noexcept {
         constexpr double bytes_in_kb = 1024;
         constexpr double bytes_in_mb = bytes_in_kb * 1024;
         constexpr double bytes_in_gb = bytes_in_mb * 1024;

         if(bytes >= bytes_in_gb){
                  return {bytes / bytes_in_gb,format == Conversion_Format::Speed ? "gb(s)/sec" : "gb(s)"};
         }

         if(bytes >= bytes_in_mb){
                  return {bytes / bytes_in_mb,format == Conversion_Format::Speed ? "mb(s)/sec" : "mb(s)"};
         }

         if(bytes >= bytes_in_kb){
                  return {bytes / bytes_in_kb,format == Conversion_Format::Speed ? "kb(s)/sec" : "kb(s)"};
         }

         return {bytes,format == Conversion_Format::Speed ? "byte(s)/sec" : "byte(s)"};
}

inline void Download_status_tracker::bind_lifetime_with_terminate_holder() noexcept {

         auto self_lifetime_connection = connect(this,&Download_status_tracker::request_satisfied,this,[self = shared_from_this()]{
                  self->hide();
         },Qt::SingleShotConnection);

         connect(this,&Download_status_tracker::release_lifetime_from_terminate_holder,[self_lifetime_connection](){
                  disconnect(self_lifetime_connection);
         });
}

inline uint32_t Download_status_tracker::get_elapsed_seconds() const noexcept {
         return static_cast<uint32_t>(time_elapsed_.second() + time_elapsed_.minute() * 60 + time_elapsed_.hour() * 3600);
}

inline void Download_status_tracker::update_state_line() noexcept {
         constexpr std::string_view null_error_info("Download completed successfully. Press the open button to view");
         constexpr std::string_view file_write_error_info("Given file could not be opened for writing");
         constexpr std::string_view unknown_network_error_info("Unknown network error. Try restarting the download");
         
         switch(error_){
                  case Error::Null : state_line_.setText(null_error_info.data()); break;
                  case Error::File_Write : state_line_.setText(file_write_error_info.data()); break;
                  case Error::Unknown_Network : state_line_.setText(unknown_network_error_info.data()); break;
                  case Error::Custom : [[fallthrough]];
                  default : __builtin_unreachable();
         }
}

#endif // STATUS_TRACKER_HXX