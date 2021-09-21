#ifndef NETWORK_MANAGER_HXX
#define NETWORK_MANAGER_HXX

#include "utility.hxx"

#include <QNetworkAccessManager>
#include <QSet>

class Download_tracker;
class QString;
class QFile;

class Network_manager : public QNetworkAccessManager {
         Q_OBJECT
public:
         struct Url_download_resources {
                  std::shared_ptr<QFile> file_handle;
                  std::shared_ptr<Download_tracker> tracker;
                  QUrl url;
         };
// 
	Network_manager();

 	constexpr auto connection_count() const noexcept;
signals:
         void terminate() const;
	void tracker_added(Download_tracker & new_tracker) const;
         void all_trackers_destroyed() const;
public slots:
	void initiate_url_download(const util::Download_request & request) noexcept;
	void initiate_torrent_download(const bencode::Metadata & torrent_metadata) noexcept;
private:
         void configure_default_connections() noexcept;
	void configure_tracker_connections(Download_tracker & tracker) const noexcept;
	bool open_file_handle(QFile & file,Download_tracker & tracker) noexcept;
         void download_url(const Url_download_resources & resources) noexcept;
         constexpr void on_tracker_destroyed() noexcept;
         ///
	QSet<QString> open_files_;
         std::uint32_t connection_count_ = 0;
         bool terminating_ = false;
};

inline Network_manager::Network_manager(){
         configure_default_connections();
}

[[nodiscard]]
constexpr auto Network_manager::connection_count() const noexcept {
         return connection_count_;
}


constexpr void Network_manager::on_tracker_destroyed() noexcept {
         assert(connection_count_ > 0);
         --connection_count_;

         if(terminating_ && !connection_count_){
                  emit all_trackers_destroyed();
         }
}

inline void Network_manager::configure_default_connections() noexcept {

         connect(this,&Network_manager::terminate,[this]{
                  terminating_ = true;

                  if(!connection_count_){
                           emit all_trackers_destroyed();
                  }
         });
}
#endif // NETWORK_MANAGER_HXX