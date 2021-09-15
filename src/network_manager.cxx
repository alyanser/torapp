#include "network_manager.hxx"
#include "download_tracker.hxx"

#include <QNetworkReply>
#include <QFile>
         
void Network_manager::download(const Download_resources & resources){
         const auto & [file_handle,tracker,url] = resources;

         assert(!url.isEmpty());
         assert(file_handle->exists());

         auto network_reply = std::shared_ptr<QNetworkReply>(get(QNetworkRequest(url)));

         auto on_error_occured = [tracker = tracker.get(),network_reply = network_reply.get()](const auto /* error_code */){
                  tracker->set_error(network_reply->errorString());
         };

         auto on_ready_read = [tracker = tracker.get(),network_reply = network_reply.get(),file_handle = file_handle.get()]{
                  
                  if(network_reply->error() == QNetworkReply::NoError){
                           file_handle->write(network_reply->readAll());
                  }else{
                           tracker->set_error(network_reply->errorString());
                  }
         };

         auto on_finished = [network_reply,tracker = tracker,file_handle = file_handle.get()]{
                  assert(network_reply.unique());
                  assert(tracker.use_count() <= 2);

                  if(network_reply->error() != QNetworkReply::NoError){
                           tracker->set_error(network_reply->errorString());
                           file_handle->remove();
                  }

                  tracker->on_download_finished();
         };

         auto on_tracker_destroyed = [file_handle = file_handle]{
                  assert(file_handle.unique());
         };

         connect(tracker.get(),&Download_tracker::destroyed,this,on_tracker_destroyed,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::readyRead,tracker.get(),on_ready_read);
         connect(network_reply.get(),&QNetworkReply::errorOccurred,tracker.get(),on_error_occured);
         connect(network_reply.get(),&QNetworkReply::finished,tracker.get(),on_finished,Qt::SingleShotConnection);
         
         connect(tracker.get(),&Download_tracker::delete_file_permanently,file_handle.get(),qOverload<>(&QFile::remove));
         connect(tracker.get(),&Download_tracker::move_file_to_trash,file_handle.get(),qOverload<>(&QFile::moveToTrash));

         connect(this,&Network_manager::terminate,network_reply.get(),&QNetworkReply::abort);
         connect(tracker.get(),&Download_tracker::request_satisfied,network_reply.get(),&QNetworkReply::abort);

         connect(network_reply.get(),&QNetworkReply::redirected,network_reply.get(),&QNetworkReply::redirectAllowed);
         connect(network_reply.get(),&QNetworkReply::downloadProgress,tracker.get(),&Download_tracker::download_progress_update);
         connect(network_reply.get(),&QNetworkReply::uploadProgress,tracker.get(),&Download_tracker::upload_progress_update);
}