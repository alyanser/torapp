#include "network_manager.hxx"
#include "download_status_tracker.hxx"

#include <assert.hpp>
#include <QNetworkReply>
#include <QFile>
#include <QMessageBox>
         
void Network_manager::download(const QUrl & address,Download_status_tracker & tracker,std::shared_ptr<QFile> file_handle){
         Assert(!address.isEmpty());

         auto network_reply = std::shared_ptr<QNetworkReply>(get(QNetworkRequest(address)));

         const auto on_download_progress_update = [&tracker](const auto bytes_received,const auto total_bytes){
                  //todo configure the tracker
         };

         const auto on_upload_progress_update = [&tracker](const auto bytes_uploaded,const auto total_bytes){
                  //todo configure the tracker
         };

         const auto on_redirected = [network_reply = std::weak_ptr(network_reply)](const auto & /* new_url */){
                  emit network_reply.lock()->redirectAllowed();
         };

         const auto on_request_cancelled = [network_reply = std::weak_ptr(network_reply)]{
                  network_reply.lock()->abort();
         };

         const auto on_error_occured = [&tracker,network_reply = std::weak_ptr(network_reply)](const auto /* error_code */){
                  tracker.set_custom_state(network_reply.lock()->errorString());
         };

         const auto on_ready_read = [&tracker,network_reply = std::weak_ptr(network_reply),file_handle = std::weak_ptr(file_handle)]{

                  if(network_reply.lock()->error() == QNetworkReply::NoError){
                           file_handle.lock()->write(network_reply.lock()->readAll());
                  }else{
                           tracker.set_custom_state(network_reply.lock()->errorString());
                           file_handle.lock()->remove();
                  }
         };

         const auto on_finished = [&tracker,network_reply,file_handle]{
                  tracker.set_custom_state(network_reply->errorString());
         };

         connect(&tracker,&Download_status_tracker::request_cancelled,network_reply.get(),on_request_cancelled,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::errorOccurred,&tracker,on_error_occured,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::readyRead,&tracker,on_ready_read);
         connect(network_reply.get(),&QNetworkReply::finished,&tracker,on_finished,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::redirected,network_reply.get(),on_redirected,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::downloadProgress,on_download_progress_update);
         connect(network_reply.get(),&QNetworkReply::uploadProgress,on_upload_progress_update);
}