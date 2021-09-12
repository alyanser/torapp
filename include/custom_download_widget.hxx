#ifndef CUSTOM_DOWNLOAD_WIDGET_HXX
#define CUSTOM_DOWNLOAD_WIDGET_HXX

#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QUrl>
#include <QLabel>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

class Custom_download_widget : public QWidget {
         Q_OBJECT
public:
         explicit Custom_download_widget(QWidget * parent = nullptr);
private:
         void on_input_received() noexcept;

         QString default_path = QDir::currentPath() + "/new_download";
         QGridLayout central_layout_ = QGridLayout(this);
         QHBoxLayout url_layout_;
         QHBoxLayout path_layout_;
         QHBoxLayout button_layout_;
         
         QLineEdit url_line_;
         QLineEdit path_line_;

         QLabel url_label_ = QLabel("Url: ");
         QLabel path_label_ = QLabel("Path: ");

         QPushButton add_button_ = QPushButton("Add");
         QPushButton cancel_button_ = QPushButton("Cancel");
         QToolButton path_button_;

signals:
         void request_received(const QUrl & custom_url,const QString & path) const;
};

inline Custom_download_widget::Custom_download_widget(QWidget * const parent) : QWidget(parent){
         setMinimumSize(QSize(500,200));
         setWindowTitle("Custom Download");
         
         central_layout_.addLayout(&url_layout_,0,0);
         central_layout_.addLayout(&path_layout_,1,0);
         central_layout_.addLayout(&button_layout_,2,0);

         url_layout_.addWidget(&url_label_);
         url_layout_.addWidget(&url_line_);
         
         button_layout_.addWidget(&add_button_);
         button_layout_.addWidget(&cancel_button_);

         path_layout_.addWidget(&path_label_);
         path_layout_.addWidget(&path_line_);
         path_layout_.addWidget(&path_button_);

         url_label_.setBuddy(&url_line_);
         path_label_.setBuddy(&path_line_);
         
         url_line_.setPlaceholderText("Complete url");
         path_line_.setText(default_path);

         connect(&path_button_,&QToolButton::pressed,[this](){
                  path_line_.setText(QFileDialog::getExistingDirectory(this));
         });

         connect(&url_line_,&QLineEdit::returnPressed,this,&Custom_download_widget::on_input_received);
         
         connect(&add_button_,&QPushButton::clicked,this,&Custom_download_widget::on_input_received);

         connect(&cancel_button_,&QPushButton::clicked,this,[this](){
                  url_line_.clear();
                  path_line_.setText(default_path);
                  hide();
         });
}

inline void Custom_download_widget::on_input_received() noexcept {
         const auto current_url = QUrl(url_line_.text());
         const auto current_path = path_line_.text().isEmpty() ? QDir::currentPath() : path_line_.text();

         {
                  const QDir dir_check;

                  if(dir_check.exists(current_path)){
                           constexpr std::string_view warning_title("File already exists");
                           constexpr std::string_view warning_body("File already exists. Do you wish to replace the existing file?");

                           const auto response = QMessageBox::question(this,warning_title.data(),warning_body.data());

                           if(response == QMessageBox::No){
                                    return;
                           }
                  }
         }
         
         url_line_.clear();
         path_line_.setText(default_path);

         hide();
         emit request_received(current_url,current_path);
}

#endif // Custom_download_widget_HXX