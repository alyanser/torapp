#ifndef URL_INPUT_DIALOG_HXX
#define URL_INPUT_DIALOG_HXX

#include <QVBoxLayout>
#include <QFormLayout>
#include <QToolButton>
#include <QPushButton>
#include <QLineEdit>
#include <QDialog>
#include <QDir>

namespace util {
	struct Download_request;
}

class Url_input_widget : public QDialog {
         Q_OBJECT
public:
         explicit Url_input_widget(QWidget * parent = nullptr);
signals:
         void new_request_received(const util::Download_request & download_request) const;
private:
         void configure_default_connections() noexcept;
         void reset_lines() noexcept;
         void setup_tab_order() noexcept;
         void setup_layout() noexcept;
         void on_input_received() noexcept;
         ///
         QString default_path_ = QDir::currentPath();

         QVBoxLayout central_layout_ = QVBoxLayout(this);
	QFormLayout central_form_layout_;

	QLineEdit url_line_;
	QLineEdit package_name_line_;

	QHBoxLayout path_layout_;
	QLineEdit path_line_;
	QToolButton path_button_;

	QHBoxLayout button_layout_;
	QPushButton download_button_ = QPushButton("Download");
	QPushButton cancel_button_ = QPushButton("Cancel");
};

inline void Url_input_widget::reset_lines() noexcept {
         url_line_.clear();
         package_name_line_.clear();
         path_line_.setText(default_path_);

	assert(!url_line_.placeholderText().isEmpty());
	assert(!package_name_line_.placeholderText().isEmpty());
}

inline void Url_input_widget::setup_tab_order() noexcept {
         setTabOrder(&url_line_,&package_name_line_);
         setTabOrder(&package_name_line_,&path_line_);
         setTabOrder(&path_line_,&path_button_);
         setTabOrder(&path_button_,&download_button_);
         setTabOrder(&download_button_,&cancel_button_);
}

#endif // URL_INPUT_DIALOG_HXX