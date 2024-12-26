#include "network_manager.h"
#include "udp_torrent_client.h"
#include "download_tracker.h"
#include "magnet_url_parser.h"

#include <QNetworkReply>
#include <QNetworkProxy>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include <QFile>

void Network_manager::download(util::Download_resources resources, const QUrl url) noexcept {

	const auto [path, file_handle, tracker] = [&resources] {
		auto & [download_path, file_handles, download_tracker] = resources;
		assert(file_handles.size() == 1);
		return std::make_tuple(std::move(download_path), file_handles.front(), download_tracker);
	}();

	tracker->set_restored_byte_count(file_handle->size());
	tracker->set_state(Download_tracker::State::Download);

	auto * const network_reply = get(QNetworkRequest(url));
	assert(network_reply->parent());

	connect(network_reply, &QNetworkReply::readyRead, tracker, [network_reply, tracker = tracker, file_handle = file_handle] {
		if(!file_handle->exists()) {
			tracker->set_error_and_finish(Download_tracker::Error::File_Write);
		} else if(network_reply->error() != QNetworkReply::NoError) {
			tracker->set_error_and_finish(network_reply->errorString());
		} else {
			file_handle->write(network_reply->readAll());
		}
	});

	connect(network_reply, &QNetworkReply::finished, tracker, [tracker = tracker, file_handle = file_handle, network_reply] {
		if(network_reply->error() == QNetworkReply::NoError) {
			tracker->set_error_and_finish(Download_tracker::Error::Null);
		} else {
			tracker->set_error_and_finish(network_reply->errorString());
		}

		network_reply->deleteLater();
		file_handle->deleteLater();
	});

	connect(network_reply, &QNetworkReply::downloadProgress, tracker, &Download_tracker::download_progress_update);
	connect(network_reply, &QNetworkReply::uploadProgress, tracker, &Download_tracker::set_upload_byte_count);
	connect(tracker, &Download_tracker::delete_files_permanently, file_handle, static_cast<bool (QFile::*)()>(&QFile::remove));
	connect(tracker, &Download_tracker::move_files_to_trash, file_handle, static_cast<bool (QFile::*)()>(&QFile::moveToTrash));
}

void Network_manager::download(util::Download_resources resources, bencode::Metadata torrent_metadata, QByteArray info_sha1_hash) noexcept {
	auto & tracker_urls = torrent_metadata.announce_url_list;

	if(std::ranges::find(std::as_const(tracker_urls), torrent_metadata.announce_url) == tracker_urls.cend()) {
		tracker_urls.insert(tracker_urls.begin(), torrent_metadata.announce_url);
	}

	auto [first, last] = std::ranges::remove_if(tracker_urls, [](const std::string & tracker_url) {
		return QUrl(tracker_url.data()).scheme() != "udp";
	});

	tracker_urls.erase(first, last);

	if(!tracker_urls.empty()) {
		[[maybe_unused]]
		auto * const udp_client = new Udp_torrent_client(std::move(torrent_metadata), std::move(resources), std::move(info_sha1_hash), this);
	} else {
		emit resources.tracker->download_dropped();

		std::ranges::for_each(resources.file_handles, [](auto * const file_handle) {
			file_handle->deleteLater();
		});

		QMessageBox::critical(nullptr, "No support", "Torapp doesn't support TCP trackers yet :(");
	}
}

void Network_manager::download(QString dl_path, magnet::Metadata torrent_metadata, Download_tracker * const tracker) noexcept {
	assert(tracker);
	assert(!torrent_metadata.tracker_urls.empty());
	assert(!dl_path.isEmpty());

	auto * const udp_client = new Udp_torrent_client(std::move(torrent_metadata), {std::move(dl_path), {}, tracker}, this);

	connect(udp_client, &Udp_torrent_client::new_download_requested, this, &Network_manager::new_download_requested);
}
