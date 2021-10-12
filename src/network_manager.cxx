#include "network_manager.hxx"
#include "udp_torrent_client.hxx"
#include "download_tracker.hxx"

#include <QNetworkReply>
#include <QNetworkProxy>
#include <QPointer>
#include <QTimer>
#include <QFile>
#include <QPair>

void Network_manager::download(util::Download_resources resources,const QUrl url) noexcept {
         ++download_cnt_;

         const auto [path,file_handle,tracker] = [&resources]{
                  auto & [download_path,file_handles,download_tracker] = resources;
                  assert(file_handles.size() == 1);
                  return std::make_tuple(std::move(download_path),file_handles.front(),download_tracker);
         }();

         tracker->set_downloaded_bytes_offset(file_handle->size());
         tracker->download_progress_update(0,-1);

         const QPointer network_reply = [this,url,file_handle = file_handle]{
                  QNetworkRequest network_request(url);
                  network_request.setRawHeader("Range","bytes=" + QByteArray::number(file_handle->size()) + '-');
                  return get(network_request);
         }();

         connect(network_reply,&QNetworkReply::finished,tracker,[tracker = tracker,file_handle = file_handle,network_reply]{

                  if(network_reply->error() == QNetworkReply::NoError){
                           tracker->switch_to_finished_state();
                  }else{
                           file_handle->remove();
                           tracker->set_error_and_finish(network_reply->errorString());
                  }

                  network_reply->deleteLater();
                  file_handle->deleteLater();
         });

         connect(network_reply,&QNetworkReply::readyRead,file_handle,[tracker = tracker,network_reply,file_handle = file_handle]{

                  if(file_handle->exists() && network_reply->error() == QNetworkReply::NoError){
                           assert(!file_handle->fileName().isEmpty());
                           file_handle->write(network_reply->readAll());
                  }else{
                           file_handle->remove();
                           tracker->set_error_and_finish(network_reply->errorString());
                  }
         });

         connect(network_reply,&QNetworkReply::errorOccurred,tracker,[tracker = tracker,network_reply]{
                  tracker->set_error_and_finish(network_reply->errorString());
         });

         connect(tracker,&Download_tracker::request_satisfied,this,[file_handle = QPointer(file_handle),network_reply = QPointer(network_reply)]{

                  if(file_handle){
                           file_handle->deleteLater();
                  }

                  if(network_reply){
                           network_reply->deleteLater();
                  }
         });
         
         connect(tracker,&Download_tracker::delete_file_permanently,file_handle,qOverload<>(&QFile::remove));
         connect(tracker,&Download_tracker::move_file_to_trash,file_handle,qOverload<>(&QFile::moveToTrash));

         connect(network_reply,&QNetworkReply::downloadProgress,tracker,&Download_tracker::download_progress_update);
         connect(network_reply,&QNetworkReply::uploadProgress,tracker,&Download_tracker::upload_progress_update);

         connect(network_reply,&QNetworkReply::redirected,&QNetworkReply::redirectAllowed);
}

void Network_manager::download(util::Download_resources resources,const bencode::Metadata & torrent_metadata) noexcept {
         
         if(const auto protocol = QUrl(torrent_metadata.announce_url.data()).scheme();protocol == "udp"){
                  ++download_cnt_;
                  [[maybe_unused]] auto * const udp_client = new Udp_torrent_client(torrent_metadata,std::move(resources),this);
         }else{
                  qDebug() << "unrecognized protocol : " << protocol;
         }
}