#ifndef NETWORK_MANAGER_HXX
#define NETWORK_MANAGER_HXX

#include <QNetworkAccessManager>

class Download_tracker;
class QFile;

class Network_manager : public QNetworkAccessManager {
         Q_OBJECT
public:
         struct Download_resources {
                  std::shared_ptr<QFile> file_handle;
                  std::shared_ptr<Download_tracker> tracker;
                  QUrl url;
         };

         Network_manager();

 	constexpr auto connection_count() const noexcept;
         constexpr void increment_connection_count() noexcept;
         void download(const Download_resources & resources) noexcept;
signals:
         void terminate() const;
         void all_trackers_destroyed() const;
public slots:
         constexpr void on_tracker_destroyed() noexcept;
private:
         void configure_default_connections() noexcept;
         ///
         uint32_t connection_count_ = 0;
         bool terminating_ = false;
};

inline Network_manager::Network_manager(){
         configure_default_connections();
}

[[nodiscard]]
constexpr auto Network_manager::connection_count() const noexcept {
         return connection_count_;
}

constexpr void Network_manager::increment_connection_count() noexcept {
         ++connection_count_;
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