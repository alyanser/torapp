#ifndef NETWORK_MANAGER_HXX
#define NETWORK_MANAGER_HXX

#include <QNetworkAccessManager>

class Download_status_tracker;
class QFile;

class Network_manager : public QNetworkAccessManager {
         Q_OBJECT
public:
         void download(const QUrl & address,Download_status_tracker & tracker,std::shared_ptr<QFile> file_handle);
signals:
         void download_finished() const;
};

#endif // NETWORK_MANAGER_HXX