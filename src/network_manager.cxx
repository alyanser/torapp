#include "network_manager.hxx"
#include "download_status_tracker.hxx"

#include <QNetworkReply>
#include <QFile>
#include <QMessageBox>
         
void Network_manager::download(const QUrl & address,std::shared_ptr<Download_status_tracker> tracker,std::shared_ptr<QFile> file_handle){
         const QNetworkRequest network_request(address);

         auto network_reply = std::shared_ptr<QNetworkReply>(get(network_request));

         const auto on_ready_read = [network_reply = network_reply.get(),tracker = tracker.get(),file_handle = file_handle.get()]{

                  if(network_reply->error() == QNetworkReply::NoError){
                           file_handle->write(network_reply->readAll());
                  }else{
                           //todo clear the file
                  }
         };

         connect(network_reply.get(),&QNetworkReply::readyRead,this,on_ready_read);

         connect(network_reply.get(),&QNetworkReply::finished,this,[network_reply,tracker,file_handle]{

         },Qt::SingleShotConnection);
}