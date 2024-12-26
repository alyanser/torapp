#include "file_allocator.h"

#include <bencode_parser.h>
#include <QMessageBox>
#include <QFile>
#include <QUrl>
#include <QDir>

auto File_allocator::open_file_handles(const QString & dir_path, const bencode::Metadata & torrent_metadata) noexcept -> std::expected<File_pointers, Error> {

	if(torrent_metadata.file_info.empty() || dir_path.isEmpty()) {
		return std::unexpected(Error::Invalid_Request);
	}

	QDir dir(dir_path);

	if(!dir.mkpath(dir.path())) {
		return std::unexpected(Error::Permissions);
	}

	std::vector<std::unique_ptr<QFile>> temp_file_handles;
	temp_file_handles.reserve(torrent_metadata.file_info.size());

	for(const auto & [torrent_file_path, torrent_file_size] : torrent_metadata.file_info) {
		QFileInfo file_info(dir, torrent_file_path.data());
		dir.mkpath(file_info.absolutePath());

		auto & file_handle = temp_file_handles.emplace_back(std::make_unique<QFile>(file_info.absoluteFilePath(), this));

		if(!file_handle->open(QFile::ReadWrite)) {
			return std::unexpected(Error::Permissions);
		}
	}

	QList<QFile *> file_handles(static_cast<qsizetype>(temp_file_handles.size()));

	std::ranges::transform(temp_file_handles, file_handles.begin(), [](auto & file_handle) {
		return file_handle.release();
	});

	return file_handles;
}

auto File_allocator::open_file_handles(const QString & file_path, const QUrl url) noexcept -> std::expected<File_pointers, Error> {

	if(file_path.isEmpty() || !url.isValid()) {
		return std::unexpected(Error::Invalid_Request);
	}

	auto file_handle = std::make_unique<QFile>(file_path, this);

	if(!file_handle->open(QFile::ReadWrite)) {
		return std::unexpected(Error::Permissions);
	}

	return QList{file_handle.release()};
}