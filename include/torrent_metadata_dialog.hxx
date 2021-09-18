#ifndef TORRENT_METADATA_DIALOG_HXX
#define TORRENT_METADATA_DIALOG_HXX

#include <bencode_parser.hxx>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFormLayout>

struct Torrent_metadata {
	std::string name = "N/A";
	std::string announce = "N/A";
	std::string announce_list = "N/A";
	std::string created_by = "N/A";
	std::string creation_date = "N/A";
	std::string comment = "N/A";
	std::string encoding = "N/A";
	std::string path = "N/A";
	std::string length = "N/A";
	std::string pieces = "N/A";
	std::vector<std::string> info_dict;
};

class Torrent_metadata_dialog : public QDialog {
public:
	explicit Torrent_metadata_dialog(const QString & file_path,QWidget * parent = nullptr);
private:
	void extract_metadata(const QString & file_path) noexcept;
	void setup_layout() noexcept;
	void setup_display(const Torrent_metadata & metadata) noexcept;
	void configure_default_connections() noexcept;
	///
	QHBoxLayout central_layout_ = QHBoxLayout(this);
	QFormLayout form_layout_;
	QLabel torrent_name_label_;
	QLabel created_by_label_;
	QLabel creation_date_label_;
	QLabel comment_label_;
	QLabel encoding_label_;
	QLabel torrent_path_label_;

	QPushButton show_announce_button_ = QPushButton("Show announace");

	QPushButton begin_button_ = QPushButton("Begin Download");
	QPushButton cancel_button_ = QPushButton("Cancel");
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
		metadata.created_by = std::any_cast<std::string>(itr->second);
	}

	if(auto itr = parsed_content.find("encoding");itr != parsed_content.end()){
		metadata.encoding = std::any_cast<std::string>(itr->second);
	}
	
	if(auto itr = parsed_content.find("announce");itr != parsed_content.end()){
		metadata.announce = std::any_cast<std::string>(itr->second);
	}

	if(auto itr = parsed_content.find("comment");itr != parsed_content.end()){
		metadata.comment = std::any_cast<std::string>(itr->second);
	}

	if(auto itr = parsed_content.find("announce-list");itr != parsed_content.end()){
		//todo
		// result.name = std::any_cast<std::string>(itr->second);
	}

	if(auto itr = parsed_content.find("info");itr != parsed_content.end()){
		// result.name = std::any_cast<std::string>(itr->second);
	}

	setup_display(metadata);
}

inline void Torrent_metadata_dialog::setup_layout() noexcept {
	central_layout_.addLayout(&form_layout_);

	form_layout_.insertRow(form_layout_.rowCount(),"Name",&torrent_name_label_);
	form_layout_.insertRow(form_layout_.rowCount(),"Name",&torrent_name_label_);
	form_layout_.insertRow(form_layout_.rowCount(),"Name",&torrent_name_label_);
	form_layout_.insertRow(form_layout_.rowCount(),"Name",&torrent_name_label_);
	form_layout_.insertRow(form_layout_.rowCount(),"Name",&torrent_name_label_);
}

inline void Torrent_metadata_dialog::setup_display(const Torrent_metadata & metadata) noexcept {
	torrent_name_label_.setText(metadata.name.data());
	comment_label_.setText(metadata.comment.data());
	created_by_label_.setText(metadata.created_by.data());
	creation_date_label_.setText(metadata.creation_date.data());
	comment_label_.setText(metadata.comment.data());
	encoding_label_.setText(metadata.encoding.data());
}

inline void Torrent_metadata_dialog::configure_default_connections() noexcept {
	connect(&cancel_button_,&QPushButton::clicked,this,&Torrent_metadata_dialog::close);
}

#endif // TORRENT_METADATA_DIALOG_HXX