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

         connect(&tracker,&Download_status_tracker::request_cancelled,network_reply.get(),[network_reply = network_reply.get()]{
                  network_reply->abort();
         },Qt::SingleShotConnection);

         connect(network_reply.get(),&QNetworkReply::errorOccurred,&tracker,[&tracker,network_reply = std::weak_ptr(network_reply)]{
                  tracker.set_custom_state(network_reply.lock()->errorString());
         },Qt::SingleShotConnection);

         const auto on_ready_read = [&tracker,network_reply = std::weak_ptr(network_reply),file_handle = std::weak_ptr(file_handle)]{

                  if(network_reply.lock()->error() == QNetworkReply::NoError){
                           file_handle.lock()->write(network_reply.lock()->readAll());
                  }else{
                           tracker.set_custom_state(network_reply.lock()->errorString());
                           file_handle.lock()->remove();
                  }
         };

         connect(network_reply.get(),&QNetworkReply::finished,&tracker,[&tracker,network_reply,file_handle]{
                  Ensures(network_reply.unique());
                  Ensures(file_handle.unique());

         },Qt::SingleShotConnection);

         connect(network_reply.get(),&QNetworkReply::readyRead,&tracker,on_ready_read);
}