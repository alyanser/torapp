#ifndef DOWNLOAD_TRACKER_HXX
#define DOWNLOAD_TRACKER_HXX

#include "utility.hxx"

#include <QStackedWidget>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QTime>

class Download_tracker : public QWidget, public std::enable_shared_from_this<Download_tracker> {
         Q_OBJECT
public:
         enum class Error { 
		Null,
		File_Write,
		Unknown_Network,
		File_Lock,
		Custom
	};

         explicit Download_tracker(const util::Download_request & download_request);
	explicit Download_tracker(const bencode::Metadata & metadata);

	constexpr Error error() const noexcept;
         std::uint32_t get_elapsed_seconds() const noexcept;
         std::shared_ptr<Download_tracker> bind_lifetime() noexcept;
         void set_error_and_finish(Error new_error) noexcept;
         void set_error_and_finish(const QString & custom_error) noexcept;
         void switch_to_finished_state() noexcept;
signals:
         void request_satisfied() const;
         void release_lifetime() const;
         void retry_url_download(const util::Download_request & download_request) const;
         void retry_torrent_download(const bencode::Metadata & metadata) const;
         void delete_file_permanently() const;
         void move_file_to_trash() const;
public slots:
         void download_progress_update(std::int64_t bytes_received,std::int64_t total_bytes) noexcept;
         void upload_progress_update(std::int64_t bytes_sent,std::int64_t total_bytes) noexcept;
private:
	Download_tracker();
         void configure_default_connections() noexcept;
         void setup_layout() noexcept;
         void setup_file_status_layout() noexcept;
         void setup_network_status_layout() noexcept;
         void setup_state_widget() noexcept;
         void update_error_line() noexcept;
         ///
         Error error_ {Error::Null};
         QVBoxLayout central_layout_ {this};
         QHBoxLayout file_stat_layout_;
         QHBoxLayout network_stat_layout_;

         QHBoxLayout package_name_layout_;
         QLabel package_name_buddy_ {"Name:"};
         QLabel package_name_label_;

         QHBoxLayout download_path_layout_;
         QLabel download_path_buddy_ {"Path:"};
         QLabel download_path_label_;

         QStackedWidget state_holder_;
         QLineEdit error_line_;
         QProgressBar download_progress_bar_;

         QHBoxLayout download_quantity_layout_;
         QLabel download_quantity_buddy_ {"Downloaded:"};
         QLabel download_quantity_label_ {"0 byte (s) / 0 byte (s)"};

         QHBoxLayout upload_quantity_layout_;
         QLabel upload_quantity_buddy_ {"Uploaded:"};
         QLabel upload_quantity_label_ {"0 byte (s) / 0 byte (s)"};

         QStackedWidget terminate_buttons_holder_;
         QPushButton finish_button_ {"Finish"};
         QPushButton cancel_button_ {"Cancel"};

         QStackedWidget initiate_buttons_holder_;
         QPushButton open_button_ {"Open"};
         QPushButton retry_button_ {"Retry"};

         QHBoxLayout time_elapsed_layout_;
         QTime time_elapsed_ {0,0,1}; // 1 to prevent division by zero
         QTimer time_elapsed_timer_;
         QLabel time_elapsed_buddy_ {"Time elapsed:"};
         QLabel time_elapsed_label_ {time_elapsed_.toString() + " hh::mm::ss"};

         QHBoxLayout download_speed_layout_;
         QLabel download_speed_buddy_ {"Download Speed:"};
         QLabel download_speed_label_ {"0 bytes/sec"};

         QPushButton delete_button_ {"Delete"};;
         QPushButton open_directory_button_ {"Open directory"};
};

inline std::shared_ptr<Download_tracker> Download_tracker::bind_lifetime() noexcept {

         const auto self_lifetime_connection = connect(this,&Download_tracker::request_satisfied,this,[self = shared_from_this()]{
                  assert(self.use_count() <= 2); // other could be held by the associated network request
         },Qt::SingleShotConnection);

	assert(self_lifetime_connection);

         connect(this,&Download_tracker::release_lifetime,this,[self_lifetime_connection]{
                  disconnect(self_lifetime_connection);
         });

	return shared_from_this();
}

inline void Download_tracker::setup_layout() noexcept {
         central_layout_.addLayout(&file_stat_layout_);
         central_layout_.addWidget(&state_holder_);
         central_layout_.addLayout(&network_stat_layout_);
}

[[nodiscard]]
constexpr Download_tracker::Error Download_tracker::error() const noexcept {
         return error_;
}

[[nodiscard]]
inline std::uint32_t Download_tracker::get_elapsed_seconds() const noexcept {
         return static_cast<std::uint32_t>(time_elapsed_.second() + time_elapsed_.minute() * 60 + time_elapsed_.hour() * 3600);
}

inline void Download_tracker::set_error_and_finish(const Error new_error) noexcept {
         assert(new_error != Error::Null && new_error != Error::Custom);
         error_ = new_error;
         update_error_line();
         switch_to_finished_state();
}

inline void Download_tracker::set_error_and_finish(const QString & custom_error) noexcept {
         error_ = Error::Custom;
         error_line_.setText(custom_error);
         switch_to_finished_state();
}

inline void Download_tracker::upload_progress_update(const std::int64_t bytes_sent,const std::int64_t total_bytes) noexcept {
         upload_quantity_label_.setText(util::conversion::stringify_bytes(bytes_sent,total_bytes));
}

#endif // DOWNLOAD_TRACKER_HXX