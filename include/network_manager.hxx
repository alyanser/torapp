#pragma once

#include "util.hxx"

#include <QNetworkAccessManager>

namespace bencode {
	struct Metadata;
}

class QUrl;

class Network_manager : public QNetworkAccessManager {
         Q_OBJECT
public:
         void download(util::Download_resources resources,QUrl url) noexcept;
         void download(util::Download_resources resources,const bencode::Metadata & torrent_metadata) noexcept;
};