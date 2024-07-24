#pragma once

#include "util.h"

#include <QNetworkAccessManager>

namespace bencode {

struct Metadata;

}

namespace magnet {

struct Metadata;

}

class QUrl;

class Network_manager : public QNetworkAccessManager {
	Q_OBJECT
public:
	void download(util::Download_resources resources, QUrl url) noexcept;
	void download(util::Download_resources resources, bencode::Metadata torrent_metadata, QByteArray info_sha1_hash) noexcept;
	void download(QString dl_path, magnet::Metadata torrent_metadata, Download_tracker * tracker) noexcept;
signals:
	void new_download_requested(QString dl_path, bencode::Metadata torrent_metadata, QByteArray info_sha1_hash) const;
};