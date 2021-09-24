#include "torrent_metadata_dialog.hxx"

void Torrent_metadata_dialog::setup_layout() noexcept {
	central_layout_.addLayout(&central_form_layout_,0,0);
	
	central_form_layout_.setSpacing(10);
	
	central_form_layout_.addRow("Name",&torrent_name_label_);
	central_form_layout_.addRow("Created by",&created_by_label_);
	central_form_layout_.addRow("Creation date",&creation_date_label_);
	central_form_layout_.addRow("Announce",&announce_label_);
	central_form_layout_.addRow("Comment",&comment_label_);
	central_form_layout_.addRow("Encoding",&encoding_label_);
	central_form_layout_.addRow("Size",&torrent_length_label_);
	central_form_layout_.addRow(&file_form_layout_);
	central_form_layout_.addRow(&button_layout_);

	file_form_layout_.addRow("Files",&file_layout_);
	
	button_layout_.addWidget(&begin_download_button_);
	button_layout_.addWidget(&cancel_button_);
}

void Torrent_metadata_dialog::setup_display(const bencode::Metadata & metadata) noexcept {
	torrent_name_label_.setText(metadata.name.data());
	torrent_length_label_.setText(QString::number(metadata.piece_length) + " kbs");
	comment_label_.setText(metadata.comment.data());
	created_by_label_.setText(metadata.created_by.data());
	creation_date_label_.setText(metadata.creation_date.data());
	comment_label_.setText(metadata.comment.data());
	encoding_label_.setText(metadata.encoding.data());

	for(const auto & [file_path,file_size_kbs] : metadata.file_info){
		constexpr auto conversion_format = util::conversion::Conversion_Format::Memory;
		
		const auto file_size_bytes = file_size_kbs * 1024;
		const auto [converted_size,postfix] = util::conversion::stringify_bytes(file_size_bytes,conversion_format);

		//! consider alt
		auto * const file_label = new QLabel(this);

		file_label->setText(QString(file_path.data()) + '\t' + QString::number(converted_size) + postfix.data());
		file_layout_.addWidget(file_label);
	}
}