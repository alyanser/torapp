#include "network_manager.hxx"
#include "udp_torrent_client.hxx"
#include "download_tracker.hxx"

#include <QNetworkReply>
#include <QNetworkProxy>
#include <QFile>
#include <QTimer>
#include <QPointer>

void Network_manager::download(const Download_resources & resources,const util::Download_request & download_request) noexcept {
	++download_count_;
         const auto & [file_handles,tracker] = resources;
	assert(file_handles.size() == 1 && tracker);

	QPointer file_handle = file_handles.front();
         QPointer network_reply = get(QNetworkRequest(download_request.url));

         auto on_download_finished = [tracker = tracker,network_reply,file_handle]{

                  if(network_reply->error() == QNetworkReply::NoError){
                           tracker->switch_to_finished_state();
                  }else{
                           tracker->set_error_and_finish(network_reply->errorString());
                           file_handle->remove();
                  }

		assert(network_reply && file_handle);
		
		network_reply->deleteLater();
		file_handle->deleteLater();
         };

	connect(tracker,&Download_tracker::request_satisfied,this,[file_handle,network_reply]{

		if(file_handle){
			file_handle->deleteLater();
		}

		if(network_reply){
			network_reply->deleteLater();
		}
	});

         connect(network_reply,&QNetworkReply::readyRead,file_handle,[tracker = tracker,network_reply,file_handle]{

                  if(network_reply->error() == QNetworkReply::NoError){
                           file_handle->write(network_reply->readAll());
                  }else{
                           tracker->set_error_and_finish(network_reply->errorString());
                  }
	});

         connect(network_reply,&QNetworkReply::errorOccurred,tracker,[tracker = tracker,network_reply]{
                  tracker->set_error_and_finish(network_reply->errorString());
	});

         connect(network_reply,&QNetworkReply::finished,tracker,on_download_finished);
         
         connect(tracker,&Download_tracker::delete_file_permanently,file_handle,qOverload<>(&QFile::remove));
         connect(tracker,&Download_tracker::move_file_to_trash,file_handle,qOverload<>(&QFile::moveToTrash));

         connect(network_reply,&QNetworkReply::redirected,network_reply,&QNetworkReply::redirectAllowed);
         connect(network_reply,&QNetworkReply::downloadProgress,tracker,&Download_tracker::download_progress_update);
         connect(network_reply,&QNetworkReply::uploadProgress,tracker,&Download_tracker::upload_progress_update);
}

void Network_manager::download(const Download_resources & ,const bencode::Metadata & torrent_metadata) noexcept {
	const auto protocol = QUrl(torrent_metadata.announce_url.data()).scheme();

	if(protocol == "udp"){
		++download_count_;
		
		auto udp_client = std::make_shared<Udp_torrent_client>(torrent_metadata)->bind_lifetime();
		QTimer::singleShot(0,udp_client.get(),&Udp_torrent_client::send_connect_request);
	}else{
		qDebug() << "unrecognized protocol : " << protocol;
	}
}