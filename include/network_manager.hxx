#pragma once

#include "utility.hxx"

#include <QNetworkAccessManager>
#include <QSet>

class Download_tracker;
class QFile;

class Network_manager : public QNetworkAccessManager {
         Q_OBJECT
public:
         struct Download_resources {
		QString file_path;
		std::vector<QFile*> file_handles;
		Download_tracker * tracker;
         };

	void download(const Download_resources & resources,QUrl url) noexcept;
	void download(const Download_resources & resources,const bencode::Metadata & torrent_metadata) noexcept;

 	constexpr std::uint32_t download_count() const noexcept;
private:
         std::uint32_t download_count_ = 0;
         bool terminating_ = false;
};

[[nodiscard]]
constexpr std::uint32_t Network_manager::download_count() const noexcept {
         return download_count_;
}