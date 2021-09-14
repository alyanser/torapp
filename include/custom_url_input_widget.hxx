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

         QString default_path = QDir::currentPath() + "/new_download";
         QVBoxLayout central_layout_ = QVBoxLayout(this);
         QHBoxLayout url_layout_;
         QHBoxLayout path_layout_;
         QHBoxLayout button_layout_;
         
         QLineEdit url_line_;
         QLineEdit path_line_ = QLineEdit(default_path);

         QLabel url_label_ = QLabel("Url: ");
         QLabel path_label_ = QLabel("Path: ");

         QPushButton download_button_ = QPushButton("Download");
         QPushButton cancel_button_ = QPushButton("Cancel");
         QToolButton path_button_;

signals:
         void request_received(const QUrl & custom_url,const QString & path) const;
};

inline Custom_url_input_widget::Custom_url_input_widget(){
         {
                  constexpr uint32_t min_width = 640;
                  constexpr uint32_t min_height = 200;

                  setMinimumSize(QSize(min_width,min_height));
         }

         url_line_.setPlaceholderText("eg: https://www.google.com/search?q=a+b");
         path_line_.setPlaceholderText("eg: /home/user/Downloads/file.txt");

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