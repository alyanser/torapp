#pragma once

#include "utility.hxx"

#include <QNetworkAccessManager>
#include <QSettings>

namespace bencode {
	struct Metadata;
}

class Network_manager : public QNetworkAccessManager {
         Q_OBJECT
public:
         void download(util::Download_resources resources,QUrl url) noexcept;
         void download(util::Download_resources resources,const bencode::Metadata & torrent_metadata) noexcept;

         constexpr std::uint32_t download_count() const noexcept;
private:
         std::uint32_t download_cnt_ = 0;
};

[[nodiscard]]
constexpr std::uint32_t Network_manager::download_count() const noexcept {
         return download_cnt_;
}