#include "torrent_metadata_dialog.h"
#include "util.h"

#include <bencode_parser.h>
#include <QStorageInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

Torrent_metadata_dialog::Torrent_metadata_dialog(const QString & torrent_file_path, QWidget * const parent) : QDialog(parent) {
	setWindowTitle("Add New Torrent");
	setup_layout();
	extract_metadata(torrent_file_path);
	configure_default_connections();

	path_line_.setText(QFileInfo(torrent_file_path).absolutePath() + '/');

	file_info_label_.setFrameShadow(QFrame::Shadow::Sunken);
	file_info_label_.setFrameShape(QFrame::Shape::Box);
	file_info_label_.setLineWidth(3);
	file_info_scroll_area_.setWidget(&file_info_label_);
	file_info_scroll_area_.setWidgetResizable(true);

	auto make_label_text_selectable = [](const QList<std::reference_wrapper<QLabel>> & labels) {
		std::ranges::for_each(labels, [](QLabel & label) {
			label.setTextInteractionFlags(Qt::TextSelectableByMouse);
			label.setCursor(QCursor(Qt::IBeamCursor));
		});
	};

	make_label_text_selectable({torrent_name_label_, created_by_label_, creation_time_label_, comment_label_, encoding_label_, piece_length_label_, size_label_, announce_label_, file_info_label_});
}

void Torrent_metadata_dialog::configure_default_connections() noexcept {
	connect(&cancel_button_, &QPushButton::clicked, this, &Torrent_metadata_dialog::reject);

	connect(&path_button_, &QToolButton::clicked, this, [this] {
		if(const auto selected_directory = QFileDialog::getExistingDirectory(this); !selected_directory.isEmpty()) {
			path_line_.setText(selected_directory);
		}
	});
}

void Torrent_metadata_dialog::setup_layout() noexcept {
	central_layout_.addLayout(&central_form_layout_, 0, 0);

	central_form_layout_.setSpacing(10);

	central_form_layout_.addRow("Name", &torrent_name_label_);
	central_form_layout_.addRow("Size", &size_label_);
	central_form_layout_.addRow("Created By", &created_by_label_);
	central_form_layout_.addRow("Creation Time", &creation_time_label_);
	central_form_layout_.addRow("Announce", &announce_label_);
	central_form_layout_.addRow("Comment", &comment_label_);
	central_form_layout_.addRow("Encoding", &encoding_label_);
	central_form_layout_.addRow("Piece Size", &piece_length_label_);
	central_form_layout_.addRow("Download Directory", &path_layout_);
	central_form_layout_.addRow("Files", &file_info_scroll_area_);
	central_form_layout_.addRow(&button_layout_);

	button_layout_.addWidget(&begin_download_button_);
	button_layout_.addWidget(&cancel_button_);

	path_layout_.addWidget(&path_line_);
	path_layout_.addWidget(&path_button_);
}

void Torrent_metadata_dialog::setup_display(const bencode::Metadata & torrent_metadata) noexcept {

	auto set_label_text = [](QLabel & label, const std::string & text) {
		label.setText(text.empty() ? "N/A" : QByteArray(text.data(), static_cast<qsizetype>(text.size())));
	};

	set_label_text(torrent_name_label_, torrent_metadata.name);
	set_label_text(announce_label_, torrent_metadata.announce_url);
	set_label_text(comment_label_, torrent_metadata.comment);
	set_label_text(created_by_label_, torrent_metadata.created_by);
	set_label_text(creation_time_label_, torrent_metadata.creation_time);
	set_label_text(encoding_label_, torrent_metadata.encoding);

	{
		const auto [converted_piece_length, postfix] = util::conversion::stringify_bytes(torrent_metadata.piece_length, util::conversion::Format::Memory);
		piece_length_label_.setText(QString::number(converted_piece_length) + ' ' + postfix.data());
	}

	{
		const auto torrent_size = torrent_metadata.single_file ? torrent_metadata.single_file_size : torrent_metadata.multiple_files_size;
		const auto [converted_size, postfix] = util::conversion::stringify_bytes(torrent_size, util::conversion::Format::Memory);
		size_label_.setText(QString::number(converted_size) + ' ' + postfix.data());
	}

	std::ranges::for_each(torrent_metadata.file_info, [this](const auto & file_info) {
		const auto & [file_path, file_size] = file_info;
		const auto [converted_size, postfix] = util::conversion::stringify_bytes(file_size, util::conversion::Format::Memory);
		const auto file_label_text = QString(file_path.data()) + "\t( " + QString::number(converted_size) + ' ' + postfix.data() + " )";
		file_info_label_.text().isEmpty() ? file_info_label_.setText(file_label_text) : file_info_label_.setText(file_info_label_.text() + '\n' + file_label_text);
	});
}

void Torrent_metadata_dialog::extract_metadata(const QString & torrent_file_path) noexcept {

	auto torrent_metadata = [&torrent_file_path]() -> std::optional<bencode::Metadata> {
		try {
			return bencode::extract_metadata(bencode::parse_file(torrent_file_path.toStdString()));
		} catch(const std::exception & exception) {
			qDebug() << exception.what();
			return {};
		}
	}();

	if(!torrent_metadata) {
		QMessageBox::critical(this, "Parse error", "Given torrent file could not be parsed. Try with a different version");
		return;
	}

	setup_display(*torrent_metadata);

	connect(&begin_download_button_, &QPushButton::clicked, this, [this, torrent_metadata = std::move(torrent_metadata)]() mutable {
		const auto dir_path = [this, &torrent_metadata = std::as_const(torrent_metadata), path_line_text = path_line_.text()]() mutable -> std::optional<QString> {
			if(!QFileInfo::exists(path_line_text)) {
				const auto reply_button = QMessageBox::question(this, "Path doesn't exist", "Path doesn't exist already. Do you wish to create it?");

				if(reply_button == QMessageBox::No) {
					return {};
				}

				if(!QDir().mkpath(path_line_text)) {
					QMessageBox::critical(this, "Path creation error", "Could not create given path. Choose a different one and try again");
					return {};
				}
			}

			if(path_line_text.isEmpty() || path_line_text.back() != '/') {
				path_line_text.push_back('/');
			}

			return path_line_text += torrent_metadata->name.data();
		}();

		if(!dir_path) {
			return;
		}

		if(QFileInfo::exists(*dir_path)) {
			const auto reply_button = QMessageBox::question(this, "Already exists", "Directory already exists. Do you wish to replace it?");

			if(reply_button == QMessageBox::No) {
				return;
			}
		}

		{
			const auto torrent_size = torrent_metadata->single_file ? torrent_metadata->single_file_size : torrent_metadata->multiple_files_size;
			assert(torrent_size > 0);

			if(QStorageInfo storage_info(path_line_.text()); storage_info.bytesFree() < torrent_size) {
				QMessageBox::critical(this, "Not enough space", "Not enough space available. Choose another path and retry");
				return;
			}
		}

		accept();
		emit new_request_received(*dir_path, std::move(*torrent_metadata));
	});
}