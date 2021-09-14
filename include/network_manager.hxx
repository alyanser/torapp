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

         uint32_t downloads_count_ = 0;
         bool abort_state_ = false;

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
                  abort_state_ = true;

                  if(!downloads_count_){
                           emit terminated();
                  }
         });
}

constexpr void Network_manager::on_tracker_destroyed() noexcept {
         assert(downloads_count_ > 0);
         
         --downloads_count_;

         if(abort_state_ && !downloads_count_){
                  emit terminated();
         }
}

#endif // NETWORK_MANAGER_HXX