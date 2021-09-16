#ifndef DOWNLOAD_TRACKER_HXX
#define DOWNLOAD_TRACKER_HXX

#include <QString>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QProgressBar>
#include <QStackedWidget>
#include <QMessageBox>
#include <QTimer>
#include <QTime>
#include <QDesktopServices>
#include <utility>

struct Download_request;

class Download_tracker : public QWidget, public std::enable_shared_from_this<Download_tracker> {
         Q_OBJECT
public:
         enum class Conversion_Format { Speed, Memory };
         enum class Error { Null, File_Write, Unknown_Network, File_Lock, Custom };

         explicit Download_tracker(const Download_request & download_request);

         void bind_lifetime() noexcept;
         [[nodiscard]] auto get_elapsed_seconds() const noexcept;
         [[nodiscard]] constexpr auto error() const noexcept;
         [[nodiscard]] static auto stringify_bytes(double bytes,Conversion_Format format) noexcept;
         [[nodiscard]] static QString stringify_bytes(int64_t bytes_received,int64_t total_bytes) noexcept;
signals:
         void request_satisfied() const;
         void release_lifetime() const;
         void retry_download(const Download_request & download_request) const;
         void delete_file_permanently() const;
         void move_file_to_trash() const;
public slots:
         void set_error(Error new_error) noexcept;
         void set_error(const QString & custom_error) noexcept;
         void switch_to_finished_state() noexcept;
         void download_progress_update(int64_t bytes_received,int64_t total_bytes) noexcept;
         void upload_progress_update(int64_t bytes_sent,int64_t total_bytes) noexcept;
private:
         void configure_default_connections() noexcept;
         void setup_layout() noexcept;
         void setup_file_status_layout() noexcept;
         void setup_network_status_layout() noexcept;
         void setup_state_widget() noexcept;
         void update_error_line() noexcept;
         ///
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
         QLineEdit error_line_;
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

         QPushButton delete_button_ = QPushButton("Delete");
         QPushButton open_directory_button_ = QPushButton("Open directory");
};

inline void Download_tracker::setup_state_widget() noexcept {
         state_holder_.addWidget(&download_progress_bar_);
         state_holder_.addWidget(&error_line_);
         assert(state_holder_.currentWidget() == &download_progress_bar_);
         download_progress_bar_.setMinimum(0);
         download_progress_bar_.setValue(0);
         error_line_.setAlignment(Qt::AlignCenter);
}

inline void Download_tracker::set_error(const Error new_error) noexcept {
         assert(new_error != Error::Null && new_error != Error::Custom);
         error_ = new_error;
         update_error_line();
}

inline void Download_tracker::set_error(const QString & custom_error) noexcept {
         error_ = Error::Custom;
         error_line_.setText(custom_error);
}

inline void Download_tracker::switch_to_finished_state() noexcept {
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

inline void Download_tracker::upload_progress_update(const int64_t bytes_sent,const int64_t total_bytes) noexcept {
         upload_quantity_label_.setText(stringify_bytes(bytes_sent,total_bytes));
}

inline auto Download_tracker::stringify_bytes(const double bytes,const Conversion_Format format) noexcept {
         constexpr double bytes_in_kb = 1024;
         constexpr double bytes_in_mb = bytes_in_kb * 1024;
         constexpr double bytes_in_gb = bytes_in_mb * 1024;

         using namespace std::string_view_literals;

         if(bytes >= bytes_in_gb){
                  return std::make_pair(bytes / bytes_in_gb,format == Conversion_Format::Speed ? "gb(s)/sec"sv : "gb(s)"sv);
         }

         if(bytes >= bytes_in_mb){
                  return std::make_pair(bytes / bytes_in_mb,format == Conversion_Format::Speed ? "mb(s)/sec"sv : "mb(s)"sv);
         }

         if(bytes >= bytes_in_kb){
                  return std::make_pair(bytes / bytes_in_kb,format == Conversion_Format::Speed ? "kb(s)/sec"sv : "kb(s)"sv);
         }

         return std::make_pair(bytes,format == Conversion_Format::Speed ? "byte(s)/sec"sv : "byte(s)"sv);
}

inline void Download_tracker::bind_lifetime() noexcept {

         const auto self_lifetime_connection = connect(this,&Download_tracker::request_satisfied,this,[self = shared_from_this()]{
                  assert(self.use_count() <= 2); // other could be held by the associated network request
         },Qt::SingleShotConnection);

         connect(this,&Download_tracker::release_lifetime,this,[self_lifetime_connection]{
                  disconnect(self_lifetime_connection);
         },Qt::SingleShotConnection);
}

inline void Download_tracker::configure_default_connections() noexcept {

         auto on_cancel_button_clicked = [this]{
                  constexpr std::string_view question_title("Cancel Download");
                  constexpr std::string_view question_body("Are you sure you want to cancel the download? All download progress will be lost.");
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

inline void Download_tracker::setup_layout() noexcept {
         central_layout_.addLayout(&file_stat_layout_);
         central_layout_.addWidget(&state_holder_);
         central_layout_.addLayout(&network_stat_layout_);
}

inline auto Download_tracker::get_elapsed_seconds() const noexcept {
         return static_cast<uint32_t>(time_elapsed_.second() + time_elapsed_.minute() * 60 + time_elapsed_.hour() * 3600);
}

constexpr auto Download_tracker::error() const noexcept {
         return error_;
}

inline void Download_tracker::update_error_line() noexcept {
         constexpr std::string_view null_error_info("Download completed successfully. Press the open button to view");
         constexpr std::string_view file_write_error_info("Given file could not be opened for writing");
         constexpr std::string_view unknown_network_error_info("Unknown network error. Try restarting the download");
         constexpr std::string_view file_lock_error_info("Same file is held by another download. Finish that download and retry");

         switch(error_){
                  case Error::Null : error_line_.setText(null_error_info.data()); break;
                  case Error::File_Write : error_line_.setText(file_write_error_info.data()); break;
                  case Error::Unknown_Network : error_line_.setText(unknown_network_error_info.data()); break;
                  case Error::File_Lock : error_line_.setText(file_lock_error_info.data()); break;
                  case Error::Custom : [[fallthrough]];
                  default : __builtin_unreachable();
         }
}

#endif // DOWNLOAD_TRACKER_HXX