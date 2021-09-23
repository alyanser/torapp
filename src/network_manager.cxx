#include "network_manager.hxx"
#include "udp_torrent_client.hxx"
#include "download_tracker.hxx"

#include <QNetworkReply>
#include <QNetworkProxy>
#include <QFile>
#include <QTimer>

bool Network_manager::open_file_handle(QFile & file_handle,Download_tracker & tracker) noexcept {
	constexpr auto failure = false;
	constexpr auto success = true;
	
	if(open_files_.contains(file_handle.fileName())){
		tracker.set_error_and_finish(Download_tracker::Error::File_Lock);
		return failure;
	}

	if(!file_handle.open(QFile::WriteOnly | QFile::Truncate)){
		tracker.set_error_and_finish(Download_tracker::Error::File_Write);
		return failure;
	}

	open_files_.insert(file_handle.fileName());

	connect(&file_handle,&QFile::destroyed,[&open_files_ = open_files_,file_name = file_handle.fileName()]{
		const auto file_itr = open_files_.find(file_name);
		assert(file_itr != open_files_.end());
		open_files_.erase(file_itr);
	});

	return success;
}

void Network_manager::configure_tracker_connections(Download_tracker & tracker) const noexcept {
         connect(this,&Network_manager::terminate,&tracker,&Download_tracker::release_lifetime);
         connect(&tracker,&Download_tracker::destroyed,this,&Network_manager::on_tracker_destroyed);
	connect(&tracker,&Download_tracker::retry_url_download,this,&Network_manager::initiate_url_download);
	connect(&tracker,&Download_tracker::retry_torrent_download,this,&Network_manager::initiate_torrent_download);
}

void Network_manager::initiate_url_download(const util::Download_request & download_request) noexcept {
	assert(!download_request.download_path.isEmpty());
         assert(!download_request.url.toString().isEmpty());

	++connection_count_;
	
         auto file_handle = std::make_shared<QFile>(download_request.download_path + '/' + download_request.package_name);
         auto tracker = std::make_shared<Download_tracker>(download_request)->bind_lifetime();

	configure_tracker_connections(*tracker);
	emit tracker_added(*tracker);

	if(open_file_handle(*file_handle,*tracker)){
                  download_url({file_handle,tracker,download_request.url});
	}
}

void Network_manager::download_url(const Url_download_resources & resources) noexcept {
         const auto & [file_handle,tracker,url] = resources;

         assert(file_handle->exists());

         auto network_reply = std::shared_ptr<QNetworkReply>(get(QNetworkRequest(url)));

         auto on_ready_read = [tracker = tracker.get(),network_reply = network_reply.get(),file_handle = file_handle.get()]{
                  
                  if(network_reply->error() == QNetworkReply::NoError){
                           file_handle->write(network_reply->readAll());
                  }else{
                           tracker->set_error_and_finish(network_reply->errorString());
                  }
         };

         auto on_download_finished = [network_reply,tracker = tracker,file_handle = file_handle]{
                  assert(network_reply.unique());
                  assert(file_handle.unique());
                  assert(tracker.use_count() <= 2);

                  if(network_reply->error() == QNetworkReply::NoError){
                           tracker->switch_to_finished_state();
                  }else{
                           tracker->set_error_and_finish(network_reply->errorString());
                           file_handle->remove();
                  }
         };

         auto on_error_occured = [tracker = tracker.get(),network_reply = network_reply.get()](const auto /* error_code */){
                  tracker->set_error_and_finish(network_reply->errorString());
         };

         connect(network_reply.get(),&QNetworkReply::finished,tracker.get(),on_download_finished,Qt::SingleShotConnection);
         connect(network_reply.get(),&QNetworkReply::readyRead,tracker.get(),on_ready_read);
         connect(network_reply.get(),&QNetworkReply::errorOccurred,tracker.get(),on_error_occured);
         
         connect(tracker.get(),&Download_tracker::delete_file_permanently,file_handle.get(),qOverload<>(&QFile::remove));
         connect(tracker.get(),&Download_tracker::move_file_to_trash,file_handle.get(),qOverload<>(&QFile::moveToTrash));

         connect(this,&Network_manager::terminate,network_reply.get(),&QNetworkReply::abort);
         connect(tracker.get(),&Download_tracker::request_satisfied,network_reply.get(),&QNetworkReply::abort);

         connect(network_reply.get(),&QNetworkReply::redirected,network_reply.get(),&QNetworkReply::redirectAllowed);
         connect(network_reply.get(),&QNetworkReply::downloadProgress,tracker.get(),&Download_tracker::download_progress_update);
         connect(network_reply.get(),&QNetworkReply::uploadProgress,tracker.get(),&Download_tracker::upload_progress_update);
}

void Network_manager::initiate_torrent_download(const bencode::Metadata & torrent_metadata) noexcept {
	const auto protocol = QUrl(torrent_metadata.announce_url.data()).scheme();

	if(protocol == "udp"){
		auto udp_client = std::make_shared<Udp_torrent_client>(torrent_metadata)->bind_lifetime();
		QTimer::singleShot(0,udp_client.get(),&Udp_torrent_client::send_connect_requests);
	}else{
		//todo inform the tracker
		
		qDebug() << "unrecognized protocol : " << protocol;
	}
}