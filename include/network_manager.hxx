#pragma once

#include "utility.hxx"

#include <QNetworkAccessManager>
#include <QSet>

class Download_tracker;
class QFile;

namespace util {
	struct Download_resources;
}

namespace bencode {
	struct Metadata;
}

class Network_manager : public QNetworkAccessManager {
         Q_OBJECT
public:
         void download(util::Download_resources resources,QUrl url) noexcept;
         void download(util::Download_resources resources,const bencode::Metadata & torrent_metadata) noexcept;

          constexpr std::uint32_t download_cnt() const noexcept;
private:
         std::uint32_t download_cnt_ = 0;
         bool terminating_ = false;
};

[[nodiscard]]
constexpr std::uint32_t Network_manager::download_cnt() const noexcept {
         return download_cnt_;
}