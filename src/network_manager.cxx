#include "network_manager.hxx"
#include "download_status_tracker.hxx"
#include <QNetworkReply>
#include <QFile>
#include <QMessageBox>
         
void Network_manager::new_download(const QUrl & address,std::shared_ptr<Download_status_tracker> tracker,std::shared_ptr<QFile> file_handle){
         QNetworkRequest network_request(address);

         auto network_reply = std::shared_ptr<QNetworkReply>(get(network_request));

         //todo make it onReadyRead instead of finished
         connect(network_reply.get(),&QNetworkReply::readyRead,this,[network_reply,file_handle,tracker](){

                  if(network_reply->error() == QNetworkReply::NoError){

                           if(file_handle->open(QFile::WriteOnly)){
                                    file_handle->write(network_reply->readAll());
                                    return;
                           }
                  }

                  //error here
                  
         },Qt::SingleShotConnection);
}