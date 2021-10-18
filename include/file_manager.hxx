#pragma once

#include "download_tracker.hxx"
#include "bencode_parser.hxx"

#include <QMessageBox>
#include <QFile>
#include <QDir>

class File_manager : public QObject {
         Q_OBJECT
public:
         enum class File_Error {
                  Null,
                  Already_Exists,
                  File_Lock,
                  Permissions,
         };

         Q_ENUM(File_Error);

         using handle_return_type = std::pair<File_Error,std::optional<QList<QFile *>>>;

         handle_return_type open_file_handles(const QString & path,const bencode::Metadata & torrent_metadata) noexcept;
         handle_return_type open_file_handles(const QString & path,QUrl url) noexcept;
};

inline File_manager::handle_return_type File_manager::open_file_handles(const QString & dir_path,const bencode::Metadata & torrent_metadata) noexcept {
         assert(!torrent_metadata.file_info.empty());
         assert(!dir_path.isEmpty());
         
         QDir dir(dir_path);

         if(!dir.mkpath(dir.path())){
                  return {File_Error::Permissions,{}};
         }

         std::vector<std::unique_ptr<QFile>> temp_file_handles;

         for(const auto & [torrent_file_path,torrent_file_size] : torrent_metadata.file_info){
                  QFileInfo file_info(dir,torrent_file_path.data());
                  dir.mkpath(file_info.absolutePath());

                  auto & file_handle = temp_file_handles.emplace_back(std::make_unique<QFile>(file_info.absoluteFilePath(),this));

                  if(!file_handle->open(QFile::ReadWrite)){
                           return {File_Error::Permissions,{}};
                  }
         }

         QList<QFile*> file_handles(static_cast<qsizetype>(temp_file_handles.size()));

         std::transform(temp_file_handles.begin(),temp_file_handles.end(),file_handles.begin(),[](auto & file_handle){
                  return file_handle.release();
         });

         return {File_Error::Null,std::move(file_handles)};
}

inline File_manager::handle_return_type File_manager::open_file_handles(const QString & file_path,const QUrl /* url */) noexcept {
         assert(!file_path.isEmpty());
         auto file_handle = std::make_unique<QFile>(file_path,this);

         if(!file_handle->open(QFile::ReadWrite)){
                  return {File_Error::Permissions,{}};
         }

         return {File_Error::Null,QList{file_handle.release()}};
}