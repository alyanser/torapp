#ifndef URL_INPUT_DIALOG_HXX
#define URL_INPUT_DIALOG_HXX

#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QDir>
#include <QUrl>

struct Download_request;

class Url_input_widget : public QDialog {
         Q_OBJECT
public:
         explicit Url_input_widget(QWidget * parent = nullptr);
signals:
         void new_request_received(const Download_request & download_request) const;
private:
         void configure_default_connections() noexcept;
         void reset_lines() noexcept;
         void setup_tab_order() noexcept;
         void setup_layout() noexcept;
         void on_input_received() noexcept;
         ///
         QString default_path_ = QDir::currentPath();
         QVBoxLayout central_layout_ = QVBoxLayout(this);

         QHBoxLayout url_layout_;
         QLabel url_label_ = QLabel("Url:");
         QLineEdit url_line_;

         QHBoxLayout path_layout_;
         QLabel path_label_ = QLabel("Path:");
         QLineEdit path_line_ = QLineEdit(default_path_);
         QToolButton path_button_;

         QHBoxLayout button_layout_;
         QPushButton download_button_ = QPushButton("Download");
         QPushButton cancel_button_ = QPushButton("Cancel");
         
         QHBoxLayout package_name_layout_;
         QLabel package_name_label_ = QLabel("File name:");
         QLineEdit package_name_line_;
};

inline Url_input_widget::Url_input_widget(QWidget * parent) : QDialog(parent){
         setMinimumSize(QSize(600,200));
         setWindowTitle("Custom Url");
         
         setup_layout();
         setup_tab_order();
         configure_default_connections();

         url_line_.setPlaceholderText("eg: https://www.google.com/search?q=hello+there");
         path_line_.setPlaceholderText("eg: /home/user/Downloads/");
         package_name_line_.setPlaceholderText("leaving this field empty will use the file name from url if any");
}

inline void Url_input_widget::configure_default_connections() noexcept {

         connect(&path_button_,&QToolButton::clicked,[this]{
                  const auto selected_directory = QFileDialog::getExistingDirectory(this);

                  if(!selected_directory.isEmpty()){
                           path_line_.setText(selected_directory);
                  }
         });

         connect(&cancel_button_,&QPushButton::clicked,[this]{
                  reset_lines();
                  hide();
         });

         connect(&download_button_,&QPushButton::clicked,this,&Url_input_widget::on_input_received);
         connect(&url_line_,&QLineEdit::returnPressed,this,&Url_input_widget::on_input_received);
         connect(&package_name_line_,&QLineEdit::returnPressed,this,&Url_input_widget::on_input_received);
         connect(&path_line_,&QLineEdit::returnPressed,this,&Url_input_widget::on_input_received);
}

inline void Url_input_widget::reset_lines() noexcept {
         url_line_.clear();
         package_name_line_.clear();
         path_line_.setText(default_path_);
}

inline void Url_input_widget::setup_tab_order() noexcept {
         setTabOrder(&url_line_,&package_name_line_);
         setTabOrder(&package_name_line_,&path_line_);
         setTabOrder(&path_line_,&path_button_);
         setTabOrder(&path_button_,&download_button_);
         setTabOrder(&download_button_,&cancel_button_);
}

#endif // URL_INPUT_DIALOG_HXX