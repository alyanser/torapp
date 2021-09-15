#include "network_manager.hxx"
#include "download_status_tracker.hxx"

#include <QNetworkReply>
#include <QFile>
         
void Network_manager::download(const QUrl & address,std::shared_ptr<Download_status_tracker> tracker,std::shared_ptr<QFile> file_handle){
         assert(!address.isEmpty());

         auto network_reply = std::shared_ptr<QNetworkReply>(get(QNetworkRequest(address)));

         const auto on_error_occured = [tracker = tracker.get(),network_reply = network_reply.get()](const auto /* error_code */){
                  tracker->set_error_and_finish(network_reply->errorString());
         };

         const auto on_ready_read = [tracker = tracker.get(),network_reply = network_reply.get(),file_handle = file_handle.get()]{

                  if(network_reply->error() == QNetworkReply::NoError){
                           file_handle->write(network_reply->readAll());
                  }else{
                           tracker->set_error_and_finish(network_reply->errorString());
                           file_handle->remove();
                  }
         };

         const auto on_finished = [tracker,network_reply,file_handle]{
                  
                  if(network_reply->error() != QNetworkReply::NoError){
                           tracker->set_error_and_finish(network_reply->errorString());
                  }else{
                           tracker->on_download_finished();
                  }
         };

         connect(network_reply.get(),&QNetworkReply::readyRead,tracker.get(),on_ready_read);
         connect(network_reply.get(),&QNetworkReply::errorOccurred,tracker.get(),on_error_occured);
         connect(network_reply.get(),&QNetworkReply::finished,tracker.get(),on_finished,Qt::SingleShotConnection);

         connect(this,&Network_manager::begin_termination,network_reply.get(),&QNetworkReply::abort);
         connect(tracker.get(),&Download_status_tracker::request_satisfied,network_reply.get(),&QNetworkReply::abort);
         connect(network_reply.get(),&QNetworkReply::redirected,network_reply.get(),&QNetworkReply::redirectAllowed);
         connect(network_reply.get(),&QNetworkReply::downloadProgress,tracker.get(),&Download_status_tracker::download_progress_update);
         connect(network_reply.get(),&QNetworkReply::uploadProgress,tracker.get(),&Download_status_tracker::upload_progress_update);
}