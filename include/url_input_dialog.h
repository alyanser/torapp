#pragma once

#include <QVBoxLayout>
#include <QFormLayout>
#include <QToolButton>
#include <QPushButton>
#include <QLineEdit>
#include <QDialog>
#include <QDir>

class Url_input_dialog : public QDialog {
	Q_OBJECT
public:
	explicit Url_input_dialog(QWidget * parent = nullptr);

signals:
	void new_request_received(const QString & file_path, QUrl url, QByteArray info_sha1_hash = "") const;

private:
	void configure_default_connections() noexcept;
	void reset_lines() noexcept;
	void setup_tab_order() noexcept;
	void setup_layout() noexcept;
	void on_input_received() noexcept;
	///
	QVBoxLayout central_layout_{this};
	QFormLayout central_form_layout_;
	QHBoxLayout path_layout_;
	QHBoxLayout button_layout_;
	QLineEdit url_line_;
	QLineEdit package_name_line_;
	QLineEdit path_line_;
	QPushButton download_button_{"Download"};
	QPushButton cancel_button_{"Cancel"};
	QToolButton path_button_;
	QString default_path_ = QDir::currentPath();
};