#pragma once

#include "download_tracker.hxx"
#include "bencode_parser.hxx"

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

	handle_return_type open_file_handles(const bencode::Metadata & torrent_metadata) noexcept;
	handle_return_type open_file_handles(const QString & path) noexcept;
};

inline File_manager::handle_return_type File_manager::open_file_handles(const bencode::Metadata & torrent_metadata) noexcept {
	QDir dir;
	const auto prefix = torrent_metadata.name + '/';

	if(!dir.mkdir(prefix.data())){
		return {Error::Permissions,{nullptr}};
	}

	std::vector<QFile*> file_handles;
	bool creation_success = true;

	for(const auto & [file_path,size] : torrent_metadata.file_info){
		const auto last_slash_idx = file_path.find_last_of('/');

		if(last_slash_idx == std::string::npos){
			file_handles.push_back(new QFile((prefix + file_path).data(),this));
		}else{
			const auto dir_path = file_path.substr(0,last_slash_idx + 1);
			const auto file_name = file_path.substr(last_slash_idx + 1);

			file_handles.push_back(new QFile((prefix + dir_path + file_name).data(),this));

			assert(!dir_path.empty() && dir_path.back() == '/');
			assert(!file_name.empty());
		}

		if(auto * file_handle = file_handles.back();file_handle->open(QFile::ReadWrite)){
			file_handles.push_back(file_handle);
		}else{
			creation_success = false;
			break;
		}
	}

	if(!creation_success){

		for(auto * file_handle : file_handles){
			file_handle->remove();
			file_handle->deleteLater();
		}

		return {Error::Permissions,{}};
	}

	return {Error::Null,file_handles};
}

inline File_manager::handle_return_type File_manager::open_file_handles(const QString & path) noexcept {
	auto * file_handle = new QFile(path,this);

	if(!file_handle->open(QFile::WriteOnly | QFile::Truncate)){
		file_handle->deleteLater();
		return {Error::Permissions,{nullptr}};
	}

	return {Error::Null,{file_handle}};
};