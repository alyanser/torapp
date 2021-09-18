#ifndef TORRENT_METADATA_DIALOG_HXX
#define TORRENT_METADATA_DIALOG_HXX

#include <bencode_parser.hxx>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>

struct Torrent_metadata {
	std::string name;
	std::string announce;
	std::string announce_list;
	std::string created_by;
	std::string creation_date;
	std::string comment;
	std::string encoding;
	std::string path;
	std::string length;
	std::string pieces;
	std::vector<std::string> info_dict;
};

class Torrent_metadata_dialog : public QDialog {
public:
	explicit Torrent_metadata_dialog(const QString & file_path,QWidget * parent = nullptr);
private:
	void extract_metadata(const QString & file_path) noexcept;
	void setup_layout() noexcept;
	void setup_display(const Torrent_metadata & metadata) noexcept;
	///
	QVBoxLayout central_layout_ = QVBoxLayout(this);

	QHBoxLayout name_layout_;
	QLabel name_buddy_ = QLabel("Name:");
	QLabel name_label_ = QLabel("N/A");

	QHBoxLayout created_by_layout_;
	QLabel created_by_buddy_ = QLabel("Created by:");
	QLabel created_by_label_ = QLabel("N/A");

	QHBoxLayout creation_date_layout_;
	QLabel creation_date_buddy_ = QLabel("Creation Date:");
	QLabel creation_date_label_ = QLabel("N/A");

	QHBoxLayout comment_layout_;
	QLabel comment_buddy_ = QLabel("Comment:");
	QLabel comment_label_ = QLabel("N/A");

	QHBoxLayout torrent_path_layout_;
	QLabel torrent_path_buddy_ = QLabel("Torrent Path:");
	QLabel torrent_path_label_ = QLabel("N/A");
};

inline Torrent_metadata_dialog::Torrent_metadata_dialog(const QString & file_path,QWidget * const parent) : QDialog(parent){
	setWindowTitle("Add New Torrent");
	setup_layout();
	extract_metadata(file_path);
}

inline void Torrent_metadata_dialog::extract_metadata(const QString & file_path) noexcept {
	const auto parsed_content = bencode::parse_file(file_path.toStdString());
	Torrent_metadata metadata;
	
	if(auto itr = parsed_content.find("name");itr != parsed_content.end()){
		metadata.name = std::any_cast<std::string>(itr->second);
	}

	if(auto itr = parsed_content.find("creation date");itr != parsed_content.end()){
		metadata.creation_date = std::to_string(std::any_cast<std::int64_t>(itr->second));
	}

	if(auto itr = parsed_content.find("created by");itr != parsed_content.end()){
		metadata.name = std::any_cast<std::string>(itr->second);
	}

	if(auto itr = parsed_content.find("encoding");itr != parsed_content.end()){
		metadata.name = std::any_cast<std::string>(itr->second);
	}
	
	if(auto itr = parsed_content.find("announce");itr != parsed_content.end()){
		//todo
		// result.name = std::any_cast<std::string>(itr->second);
	}

	if(auto itr = parsed_content.find("announce-list");itr != parsed_content.end()){
		//todo
		// result.name = std::any_cast<std::string>(itr->second);
	}

	if(auto itr = parsed_content.find("comment");itr != parsed_content.end()){
		//todo
		// result.name = std::any_cast<std::string>(itr->second);
	}

	if(auto itr = parsed_content.find("info");itr != parsed_content.end()){
		//todo
		// result.name = std::any_cast<std::string>(itr->second);
	}

	setup_display(metadata);
}

inline void Torrent_metadata_dialog::setup_display(const Torrent_metadata & metadata) noexcept {
	name_label_.setText(metadata.name.data());
}

inline void Torrent_metadata_dialog::setup_layout() noexcept {
	central_layout_.addLayout(&name_layout_);
	central_layout_.addLayout(&comment_layout_);
	central_layout_.addLayout(&created_by_layout_);
	central_layout_.addLayout(&creation_date_layout_);
	central_layout_.addLayout(&torrent_path_layout_);

	name_layout_.addWidget(&name_buddy_);
	name_layout_.addWidget(&name_label_);

	comment_layout_.addWidget(&comment_buddy_);
	comment_layout_.addWidget(&comment_label_);

	created_by_layout_.addWidget(&created_by_buddy_);
	created_by_layout_.addWidget(&created_by_label_);

	creation_date_layout_.addWidget(&creation_date_buddy_);
	creation_date_layout_.addWidget(&creation_date_label_);

	torrent_path_layout_.addWidget(&torrent_path_buddy_);
	torrent_path_layout_.addWidget(&torrent_path_label_);
}

#endif // TORRENT_METADATA_DIALOG_HXX