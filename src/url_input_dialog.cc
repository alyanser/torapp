#include "url_input_dialog.h"
#include "util.h"

#include <QFileDialog>
#include <QMessageBox>

Url_input_dialog::Url_input_dialog(QWidget * const parent) : QDialog(parent) {
	setFixedSize({600, 200});
	setWindowTitle("Custom Url");

	setup_layout();
	setup_tab_order();
	configure_default_connections();

	path_line_.setText(default_path_);
}

void Url_input_dialog::setup_tab_order() noexcept {
	setTabOrder(&url_line_, &package_name_line_);
	setTabOrder(&package_name_line_, &path_line_);
	setTabOrder(&path_line_, &path_button_);
	setTabOrder(&path_button_, &download_button_);
	setTabOrder(&download_button_, &cancel_button_);
}

void Url_input_dialog::configure_default_connections() noexcept {

	connect(&path_button_, &QToolButton::clicked, this, [this] {
		if(const auto selected_directory = QFileDialog::getExistingDirectory(this); !selected_directory.isEmpty()) {
			path_line_.setText(selected_directory);
		}
	});

	connect(&cancel_button_, &QPushButton::clicked, this, [this] {
		reset_lines();
		reject();
	});

	connect(&download_button_, &QPushButton::clicked, this, &Url_input_dialog::on_input_received);
	connect(&url_line_, &QLineEdit::returnPressed, this, &Url_input_dialog::on_input_received);
	connect(&package_name_line_, &QLineEdit::returnPressed, this, &Url_input_dialog::on_input_received);
	connect(&path_line_, &QLineEdit::returnPressed, this, &Url_input_dialog::on_input_received);
}

void Url_input_dialog::reset_lines() noexcept {
	url_line_.clear();
	package_name_line_.clear();
	path_line_.setText(default_path_);
}

void Url_input_dialog::setup_layout() noexcept {
	central_layout_.addLayout(&central_form_layout_);
	central_form_layout_.setSpacing(25);

	central_form_layout_.addRow("URL:", &url_line_);
	central_form_layout_.addRow("Name:", &package_name_line_);
	central_form_layout_.addRow("Path", &path_layout_);
	central_form_layout_.addRow(&button_layout_);

	path_layout_.addWidget(&path_line_);
	path_layout_.addWidget(&path_button_);

	button_layout_.addWidget(&download_button_);
	button_layout_.addWidget(&cancel_button_);
}

void Url_input_dialog::on_input_received() noexcept {
	QUrl url(url_line_.text().simplified());

	if(url.isEmpty()) {
		QMessageBox::critical(this, "Empty Url", "Url field cannot be empty");
		return;
	}

	auto dir_path = path_line_.text().simplified();

	if(dir_path.isEmpty()) {
		QMessageBox::critical(this, "Invalid Path", "Path field cannot be empty");
		return;
	}

	if(dir_path.back() != '/') {
		dir_path += '/';
	}

	auto package_name = package_name_line_.text().simplified();

	if(package_name.isEmpty()) {

		if(url.fileName().isEmpty()) {
			QMessageBox::critical(this, "Empty file name", "One of file name field or URL's file name must be non-empty");
			return;
		}

		package_name = url.fileName();
	}

	const auto file_path = dir_path + package_name;

	if(QFileInfo::exists(file_path)) {
		const auto reply_button = QMessageBox::question(this, "Already exists", "Do you wish to replace the existing file?");

		if(reply_button == QMessageBox::No) {
			return;
		}
	}

	accept();
	emit new_request_received(file_path, url);
}