#pragma once

#include <QObject>
#include <expected>

class QFile;

namespace bencode {
struct Metadata;
}

class File_allocator : public QObject {
	Q_OBJECT
public:
	enum class Error {
		File_Lock,
		Permissions,
		Invalid_Request
	};

	Q_ENUM(Error);

	using File_pointers = QList<QFile *>;

	std::expected<File_pointers, Error> open_file_handles(const QString & path, const bencode::Metadata & torrent_metadata) noexcept;
	std::expected<File_pointers, Error> open_file_handles(const QString & path, QUrl url) noexcept;
};