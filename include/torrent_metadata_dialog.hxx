#pragma once

#include "utility.hxx"

#include <bencode_parser.hxx>
#include <QPushButton>
#include <QFormLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>

class Torrent_metadata_dialog : public QDialog {
	Q_OBJECT
public:
	explicit Torrent_metadata_dialog(const QString & file_path,QWidget * parent = nullptr);
signals:
	void new_request_received(const bencode::Metadata & metadata) const;
private:
	void extract_metadata(const QString & file_path) noexcept;
	void setup_layout() noexcept;
	void setup_display(const bencode::Metadata & metadata) noexcept;
	void configure_default_connections() noexcept;
	///
	QGridLayout central_layout_ {this};
	QFormLayout central_form_layout_;

	QLabel torrent_name_label_;
	QLabel created_by_label_;
	QLabel creation_date_label_;
	QLabel comment_label_;
	QLabel encoding_label_;
	QLabel torrent_length_label_;
	QLabel announce_label_;

	QHBoxLayout button_layout_;
	QPushButton begin_download_button_ {"Begin Download"};
	QPushButton cancel_button_ {"Cancel"};

	QFormLayout file_form_layout_;
	QVBoxLayout file_layout_;
};

inline Torrent_metadata_dialog::Torrent_metadata_dialog(const QString & file_path,QWidget * const parent) : QDialog(parent){
	setWindowTitle("Add New Torrent");
	setup_layout();
	extract_metadata(file_path);
	configure_default_connections();
}

inline void Torrent_metadata_dialog::extract_metadata(const QString & file_path) noexcept {
	auto metadata = bencode::extract_metadata(bencode::parse_file(file_path.toStdString()));
	
	setup_display(metadata);

	connect(&begin_download_button_,&QPushButton::clicked,this,[this,metadata = std::move(metadata)]{
		emit new_request_received(metadata);
	});

	//! metadata moved from
}

inline void Torrent_metadata_dialog::configure_default_connections() noexcept {
	connect(&begin_download_button_,&QPushButton::clicked,this,&Torrent_metadata_dialog::accept);
	connect(&cancel_button_,&QPushButton::clicked,this,&Torrent_metadata_dialog::reject);
}