#ifndef NETWORK_MANAGER_HXX
#define NETWORK_MANAGER_HXX

#include <QNetworkAccessManager>

class Download_status_tracker;
class QFile;

class Network_manager : public QNetworkAccessManager {
         Q_OBJECT
public:
         Network_manager();

         void download(const QUrl & address,std::shared_ptr<Download_status_tracker> tracker,std::shared_ptr<QFile> file_handle);
private:
         void configure_default_connections() noexcept;

         uint32_t download_count_ = 0;
         bool aborting_ = false;

signals:
         void begin_termination() const;
         void terminated() const;

public slots:
         constexpr void on_tracker_destroyed() noexcept;
};

inline Network_manager::Network_manager(){
         configure_default_connections();
}

inline void Network_manager::configure_default_connections() noexcept {

         connect(this,&Network_manager::begin_termination,[this]{
                  aborting_ = true;

                  if(!download_count_){
                           emit terminated();
                  }
         });
}

constexpr void Network_manager::on_tracker_destroyed() noexcept {
         assert(download_count_ > 0);
         
         --download_count_;

         if(aborting_ && !download_count_){
                  emit terminated();
         }
}

#endif // NETWORK_MANAGER_HXX