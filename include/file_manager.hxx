#pragma once

#include "download_tracker.hxx"
#include "bencode_parser.hxx"

#include <QMessageBox>
#include <QFileInfo>
#include <QObject>
#include <QFile>
#include <QDir>

class File_manager : public QObject {
         Q_OBJECT
public:
         enum class Error {
                  Null,
                  Already_Exists,
                  File_Lock,
                  Permissions
         };

         using handle_return_type = std::pair<Error,std::vector<QFile*>>;

         handle_return_type open_file_handles(const QString & path,const bencode::Metadata & torrent_metadata) noexcept;
         handle_return_type open_file_handles(const QString & path,QUrl url) noexcept;
};

inline File_manager::handle_return_type File_manager::open_file_handles(const QString & path,const bencode::Metadata & torrent_metadata) noexcept {
         assert(!path.isEmpty());
         QDir dir(path);

         if(!dir.mkpath(dir.path())){
                  return {Error::Permissions,{nullptr}};
         }

         std::vector<QFile*> file_handles;

         for(const auto & [file_path,size] : torrent_metadata.file_info){
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

                  if(auto * file_handle = file_handles.back();!file_handle->open(QFile::WriteOnly | QFile::Truncate)){
                           
                           for(auto * const invalid_file_handle : file_handles){
                                    invalid_file_handle->deleteLater();
                           }

                           dir.removeRecursively();

                           return {Error::Permissions,{}};
                  }
         }

         return {Error::Null,file_handles};
}

inline File_manager::handle_return_type File_manager::open_file_handles(const QString & path,const QUrl /* url */) noexcept {
         auto * file_handle = new QFile(path,this);

         if(!file_handle->open(QFile::WriteOnly | QFile::Truncate)){
                  file_handle->deleteLater();
                  return {Error::Permissions,{nullptr}};
         }

         return {Error::Null,{file_handle}};
}