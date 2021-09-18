#ifndef TORRENT_METADATA_DIALOG_HXX
#define TORRENT_METADATA_DIALOG_HXX

#include "utility.hxx"

#include <bencode_parser.hxx>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFormLayout>
#include <QGridLayout>
#include <QCheckBox>
#include <string>
#include <vector>

class Torrent_metadata_dialog : public QDialog {
	Q_OBJECT
public:
	explicit Torrent_metadata_dialog(const QString & file_path,QWidget * parent = nullptr);
private:
	void extract_metadata(const QString & file_path) noexcept;
	void setup_layout() noexcept;
	void setup_display(const bencode::Metadata & metadata) noexcept;
	void configure_default_connections() noexcept;
	///
	QGridLayout central_layout_ = QGridLayout(this);
	QFormLayout central_form_layout_;

	QLabel torrent_name_label_;
	QLabel created_by_label_;
	QLabel creation_date_label_;
	QLabel comment_label_;
	QLabel encoding_label_;
	QLabel torrent_length_label_;
	QLabel announce_label_;

	QHBoxLayout button_layout_;
	QPushButton begin_download_button_ = QPushButton("Begin Download");
	QPushButton cancel_button_ = QPushButton("Cancel");

	QFormLayout file_form_layout_;
	QVBoxLayout file_layout_;
};

inline Torrent_metadata_dialog::Torrent_metadata_dialog(const QString & file_path,QWidget * const parent) : QDialog(parent){
	setWindowTitle("Add New Torrent");
	setup_layout();
	extract_metadata(file_path);
	configure_default_connections();
}

inline void Torrent_metadata_dialog::setup_layout() noexcept {
	central_layout_.addLayout(&central_form_layout_,0,0);
	central_form_layout_.setSpacing(10);
	
	central_form_layout_.insertRow(central_form_layout_.rowCount(),"Name",&torrent_name_label_);
	central_form_layout_.insertRow(central_form_layout_.rowCount(),"Created by",&created_by_label_);
	central_form_layout_.insertRow(central_form_layout_.rowCount(),"Creation date",&creation_date_label_);
	central_form_layout_.insertRow(central_form_layout_.rowCount(),"Announce",&announce_label_);
	central_form_layout_.insertRow(central_form_layout_.rowCount(),"Comment",&comment_label_);
	central_form_layout_.insertRow(central_form_layout_.rowCount(),"Encoding",&encoding_label_);
	central_form_layout_.insertRow(central_form_layout_.rowCount(),"Size",&torrent_length_label_);
	central_form_layout_.insertRow(central_form_layout_.rowCount(),&file_form_layout_);
	central_form_layout_.insertRow(central_form_layout_.rowCount(),&button_layout_);

	file_form_layout_.insertRow(file_form_layout_.rowCount(),"Files",&file_layout_);
	button_layout_.addWidget(&begin_download_button_);
	button_layout_.addWidget(&cancel_button_);
}

inline void Torrent_metadata_dialog::setup_display(const bencode::Metadata & metadata) noexcept {
	torrent_name_label_.setText(metadata.name.data());
	torrent_length_label_.setText(QString::number(metadata.piece_length) + " kbs");
	comment_label_.setText(metadata.comment.data());
	created_by_label_.setText(metadata.created_by.data());
	creation_date_label_.setText(metadata.creation_date.data());
	comment_label_.setText(metadata.comment.data());
	encoding_label_.setText(metadata.encoding.data());

	for(const auto & [file_path,file_size_kbs] : metadata.file_info){
		constexpr auto format = util::conversion::Conversion_Format::Memory;
		const auto file_size_bytes = file_size_kbs * 1024;
		const auto [converted_size,postfix] = util::conversion::stringify_bytes(file_size_bytes,format);
		auto * const file_label = new QLabel(this);

		file_label->setText(QString(file_path.data()) + '\t' + QString::number(converted_size) + postfix.data());
		file_layout_.addWidget(file_label);
	}
}

inline void Torrent_metadata_dialog::extract_metadata(const QString & file_path) noexcept {
	const auto metadata = bencode::extract_metadata(bencode::parse_file(file_path.toStdString()));
	setup_display(metadata);
}

inline void Torrent_metadata_dialog::configure_default_connections() noexcept {
	connect(&cancel_button_,&QPushButton::clicked,this,&Torrent_metadata_dialog::close);
}

#endif // TORRENT_METADATA_DIALOG_HXX