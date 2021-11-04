#pragma once

#include <QObject>

class QFile;

namespace bencode {
         struct Metadata;
}

class File_allocator : public QObject {
         Q_OBJECT
public:
         enum class File_Error {
                  Null,
                  File_Lock,
                  Permissions,
         };

         Q_ENUM(File_Error);

         using handle_return_type = std::pair<File_Error,std::optional<QList<QFile *>>>;

         handle_return_type open_file_handles(const QString & path,const bencode::Metadata & torrent_metadata) noexcept;
         handle_return_type open_file_handles(const QString & path,QUrl url) noexcept;
};