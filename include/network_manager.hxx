#pragma once

#include "util.hxx"

#include <QNetworkAccessManager>
#include <QSettings>

namespace bencode {
	struct Metadata;
}

class Network_manager : public QNetworkAccessManager {
         Q_OBJECT
public:
         constexpr std::int32_t download_count() const noexcept;
         void download(util::Download_resources resources,QUrl url) noexcept;
         void download(util::Download_resources resources,const bencode::Metadata & torrent_metadata) noexcept;
private:
         std::int32_t download_cnt_ = 0;
};

[[nodiscard]]
constexpr std::int32_t Network_manager::download_count() const noexcept {
         return download_cnt_;
}