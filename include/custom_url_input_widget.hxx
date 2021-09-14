#ifndef CUSTOM_URL_INPUT_WIDGET_HXX
#define CUSTOM_URL_INPUT_WIDGET_HXX

#include <QLineEdit>
#include <QUrl>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QToolButton>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

class Custom_url_input_widget : public QWidget {
         Q_OBJECT
public:
         Custom_url_input_widget();
private:
         void configure_default_connections() noexcept;
         void setup_layout() noexcept;
         void on_input_received() noexcept;

         QString default_path = QDir::currentPath() + '/';
         QVBoxLayout central_layout_ = QVBoxLayout(this);

         QHBoxLayout url_layout_;
         QLabel url_label_ = QLabel("Url: ");
         QLineEdit url_line_;

         QHBoxLayout path_layout_;
         QLabel path_label_ = QLabel("Path: ");
         QLineEdit path_line_ = QLineEdit(default_path);
         QToolButton path_button_;

         QHBoxLayout button_layout_;
         QPushButton download_button_ = QPushButton("Download");
         QPushButton cancel_button_ = QPushButton("Cancel");
         
         QHBoxLayout package_name_layout_;
         QLabel package_name_label_ = QLabel("File name: ");
         QLineEdit package_name_line_ = QLineEdit("new_download");

signals:
         void request_received(const QUrl & custom_url,const QString & path,const QString & package_name) const;
};

inline Custom_url_input_widget::Custom_url_input_widget(){
         {
                  constexpr uint32_t min_width = 640;
                  constexpr uint32_t min_height = 200;

                  setMinimumSize(QSize(min_width,min_height));
         }

         url_line_.setPlaceholderText("eg: https://www.google.com/search?q=hello+there");
         path_line_.setPlaceholderText("eg: /home/user/Downloads/");
         package_name_line_.setPlaceholderText("eg: my_file");

         setWindowTitle("Custom Url");
         setup_layout();
         configure_default_connections();
}

inline void Custom_url_input_widget::configure_default_connections() noexcept {

         const auto on_path_button_clicked = [this]{
                  const auto selected_directory = QFileDialog::getExistingDirectory(this);

                  if(!selected_directory.isEmpty()){
                           path_line_.setText(selected_directory + "/new_download");
                  }
         };

         const auto on_cancel_button_clicked = [this]{
                  url_line_.clear();
                  path_line_.setText(default_path);
                  hide();
         };

         connect(&path_button_,&QToolButton::clicked,on_path_button_clicked);
         connect(&cancel_button_,&QPushButton::clicked,on_cancel_button_clicked);
         connect(&url_line_,&QLineEdit::returnPressed,this,&Custom_url_input_widget::on_input_received);
         connect(&download_button_,&QPushButton::clicked,this,&Custom_url_input_widget::on_input_received);
}

#endif // CUSTOM_URL_INPUT_WIDGET_HXX