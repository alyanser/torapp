#include "network_manager.hxx"
#include "download_status_tracker.hxx"

#include <gsl/assert>
#include <QNetworkReply>
#include <QFile>
#include <QMessageBox>
         
void Network_manager::download(const QUrl & address,Download_status_tracker & tracker,std::shared_ptr<QFile> file_handle){
         Expects(!address.isEmpty());

         const QNetworkRequest network_request(address);

         auto network_reply = std::shared_ptr<QNetworkReply>(get(network_request));

         const auto on_ready_read = [&tracker,network_reply = network_reply.get(),file_handle = file_handle.get()]{

                  if(network_reply->error() == QNetworkReply::NoError){
                           file_handle->write(network_reply->readAll());
                  }else{
                           //todo update the error types
                           tracker.set_state(Download_status_tracker::State::Unknown_Network_Error);
                           file_handle->remove();
                  }
         };

         connect(network_reply.get(),&QNetworkReply::readyRead,this,on_ready_read);

         connect(network_reply.get(),&QNetworkReply::finished,this,[&tracker,network_reply,file_handle]{
                  Ensures(network_reply.unique());
                  Ensures(file_handle.unique());

         },Qt::SingleShotConnection);
}