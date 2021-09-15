#include "network_manager.hxx"
#include "download_tracker.hxx"

#include <QNetworkReply>
#include <QFile>
#include <QLockFile>
#include <memory>
         
void Network_manager::download(const Download_resources & resources){
         const auto & [file_handle,file_lock,tracker,address] = resources;

         assert(!address.isEmpty());
         assert(file_handle->exists());
         assert(file_lock->isLocked());

         auto network_reply = std::shared_ptr<QNetworkReply>(get(QNetworkRequest(address)));

         const auto on_error_occured = [tracker = tracker.get(),network_reply = network_reply.get()](const auto /* error_code */){
                  tracker->set_error_and_finish(network_reply->errorString());
         };

         const auto on_ready_read = [tracker = tracker.get(),network_reply = network_reply.get(),file_handle = file_handle.get()]{

                  if(network_reply->error() == QNetworkReply::NoError){
                           file_handle->write(network_reply->readAll());
                  }else{
                           tracker->set_error_and_finish(network_reply->errorString());
                  }
         };

         const auto on_finished = [network_reply,tracker = tracker,file_handle = file_handle.get()]{
                  
                  if(network_reply->error() == QNetworkReply::NoError){
                           tracker->download_finished();
                  }else{
                           tracker->set_error_and_finish(network_reply->errorString());
                           file_handle->remove();
                  }
         };

         const auto on_tracker_destroyed = [file_handle = file_handle,file_lock = file_lock]{};

         connect(network_reply.get(),&QNetworkReply::readyRead,tracker.get(),on_ready_read);
         connect(network_reply.get(),&QNetworkReply::errorOccurred,tracker.get(),on_error_occured);

         connect(network_reply.get(),&QNetworkReply::finished,tracker.get(),on_finished,Qt::SingleShotConnection);
         connect(tracker.get(),&Download_tracker::destroyed,this,on_tracker_destroyed,Qt::SingleShotConnection);
         connect(tracker.get(),&Download_tracker::delete_file_permanently,file_handle.get(),qOverload<>(&QFile::remove));
         connect(tracker.get(),&Download_tracker::move_file_to_trash,file_handle.get(),qOverload<>(&QFile::moveToTrash));

         connect(this,&Network_manager::terminate,network_reply.get(),&QNetworkReply::abort);
         connect(tracker.get(),&Download_tracker::request_satisfied,network_reply.get(),&QNetworkReply::abort);
         connect(network_reply.get(),&QNetworkReply::redirected,network_reply.get(),&QNetworkReply::redirectAllowed);
         connect(network_reply.get(),&QNetworkReply::downloadProgress,tracker.get(),&Download_tracker::download_progress_update);
         connect(network_reply.get(),&QNetworkReply::uploadProgress,tracker.get(),&Download_tracker::upload_progress_update);
}