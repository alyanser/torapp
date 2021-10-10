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
                  Not_Enough_Space
         };

         using handle_return_type = std::pair<File_Error,std::optional<std::vector<QFile *>>>;

         handle_return_type open_file_handles(const QString & path,const bencode::Metadata & torrent_metadata) noexcept;
         handle_return_type open_file_handles(const QString & path,QUrl url) noexcept;
};

inline File_manager::handle_return_type File_manager::open_file_handles(const QString & path,const bencode::Metadata & torrent_metadata) noexcept {
         assert(!path.isEmpty());
         QDir dir(path);

         if(!dir.mkpath(dir.path())){
                  return {File_Error::Permissions,{}};
         }

         std::vector<QFile *> file_handles;

         auto remove_file_handles = [&file_handles,&dir]{

                  for(auto * const invalid_file_handle : file_handles){
                           invalid_file_handle->deleteLater();
                  }

                  dir.removeRecursively();
         };

         for(const auto & [file_path,file_size] : torrent_metadata.file_info){
                  const auto last_slash_idx = file_path.find_last_of('/');

                  if(last_slash_idx == std::string::npos){
                           file_handles.push_back(new QFile(dir.path() + '/' + file_path.data(),this));
                  }else{
                           const auto new_dir_path = file_path.substr(0,last_slash_idx);
                           const auto file_name = file_path.substr(last_slash_idx + 1);
                           assert(!file_name.empty());

                           dir.mkpath(new_dir_path.data());
                           file_handles.push_back(new QFile(dir.path() + '/' + new_dir_path.data() + '/' + file_name.data(),this));
                  }

                  assert(!file_handles.empty());
                  auto * const file_handle = file_handles.back();

                  if(!file_handle->open(QFile::ReadWrite | QFile::Truncate)){
                           remove_file_handles();
                           return {File_Error::Permissions,{}};
                  }

                  if(!file_handle->resize(static_cast<qsizetype>(file_size))){
                           remove_file_handles();
                           return {File_Error::Not_Enough_Space,{}};
                  }
         }

         assert(!file_handles.empty());

         return {File_Error::Null,file_handles};
}

inline File_manager::handle_return_type File_manager::open_file_handles(const QString & path,const QUrl /* url */) noexcept {
         auto * const file_handle = new QFile(path,this);

         if(!file_handle->open(QFile::WriteOnly | QFile::Truncate)){
                  file_handle->deleteLater();
                  return {File_Error::Permissions,{}};
         }

         return {File_Error::Null,std::vector{file_handle}};
}