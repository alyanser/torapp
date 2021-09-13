#include "network_manager.hxx"
#include "download_status_tracker.hxx"

#include <QNetworkReply>
#include <QFile>
#include <QMessageBox>
         
void Network_manager::download(const QUrl & address,std::shared_ptr<Download_status_tracker> tracker,std::shared_ptr<QFile> file_handle){
         assert(!address.isEmpty());

         ++downloads_count_;

         auto network_reply = std::shared_ptr<QNetworkReply>(get(QNetworkRequest(address)));

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
                  }

                  tracker->on_download_finished();
         };

         connect(this,&Network_manager::begin_termination,network_reply.get(),&QNetworkReply::abort);
         connect(this,&Network_manager::begin_termination,tracker.get(),&Download_status_tracker::release_lifetime_from_close_button);
         connect(tracker.get(),&Download_status_tracker::destroyed,this,&Network_manager::on_tracker_destroyed);

         connect(network_reply.get(),&QNetworkReply::readyRead,tracker.get(),on_ready_read);
         connect(network_reply.get(),&QNetworkReply::errorOccurred,tracker.get(),on_error_occured,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::finished,tracker.get(),on_finished,Qt::SingleShotConnection);

         connect(tracker.get(),&Download_status_tracker::request_satisfied,network_reply.get(),&QNetworkReply::abort,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::redirected,network_reply.get(),&QNetworkReply::redirectAllowed,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::downloadProgress,tracker.get(),&Download_status_tracker::download_progress_update);
         connect(network_reply.get(),&QNetworkReply::uploadProgress,tracker.get(),&Download_status_tracker::upload_progress_update);
}