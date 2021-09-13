#include "network_manager.hxx"
#include "download_status_tracker.hxx"

#include <QNetworkReply>
#include <QFile>
#include <QMessageBox>
         
void Network_manager::download(const QUrl & address,std::shared_ptr<Download_status_tracker> tracker,std::shared_ptr<QFile> file_handle){
         assert(!address.isEmpty());

         auto network_reply = std::shared_ptr<QNetworkReply>(get(QNetworkRequest(address)));

         const auto on_redirected = [network_reply = network_reply.get()](const auto & /* new_url */){
                  emit network_reply->redirectAllowed();
         };

         const auto on_request_satisfied = [network_reply = network_reply.get()]{
                  network_reply->abort();
         };

         const auto on_error_occured = [tracker = tracker.get(),network_reply = network_reply.get()](const auto /* error_code */){
                  tracker->set_custom_state(network_reply->errorString());
         };

         const auto on_ready_read = [tracker = tracker.get(),network_reply = network_reply.get(),file_handle = file_handle.get()]{

                  if(network_reply->error() == QNetworkReply::NoError){
                           file_handle->write(network_reply->readAll());
                  }else{
                           tracker->set_custom_state(network_reply->errorString());
                           file_handle->remove();
                  }
         };

         const auto on_finished = [tracker,network_reply,file_handle]{
                  
                  if(network_reply->error()){
                           tracker->set_custom_state(network_reply->errorString());
                  }else{
                           tracker->set_misc_state(Download_status_tracker::Misc_State::Download_Finished);
                           tracker->on_download_finished();
                  }

                  tracker->on_download_finished();
         };

         connect(tracker.get(),&Download_status_tracker::request_satisfied,network_reply.get(),on_request_satisfied,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::errorOccurred,tracker.get(),on_error_occured,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::readyRead,tracker.get(),on_ready_read);
         connect(network_reply.get(),&QNetworkReply::finished,tracker.get(),on_finished,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::redirected,network_reply.get(),on_redirected,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::downloadProgress,tracker.get(),&Download_status_tracker::download_progress_update);
         connect(network_reply.get(),&QNetworkReply::uploadProgress,tracker.get(),&Download_status_tracker::upload_progress_update);
}