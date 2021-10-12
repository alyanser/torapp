#include "url_input_dialog.hxx"
#include "utility.hxx"

#include <QFileDialog>
#include <QMessageBox>

Url_input_dialog::Url_input_dialog(QWidget * const parent) 
         : QDialog(parent)
{
         setFixedSize({600,200});
         setWindowTitle("Custom Url");
         
         setup_layout();
         setup_tab_order();
         configure_default_connections();

         url_line_.setPlaceholderText("eg: https://www.google.com/search?q=hello+there");
         path_line_.setText(default_path_);
         package_name_line_.setPlaceholderText("leaving this field empty will use the file name from url if any");
}

void Url_input_dialog::setup_tab_order() noexcept {
         setTabOrder(&url_line_,&package_name_line_);
         setTabOrder(&package_name_line_,&path_line_);
         setTabOrder(&path_line_,&path_button_);
         setTabOrder(&path_button_,&download_button_);
         setTabOrder(&download_button_,&cancel_button_);
}

void Url_input_dialog::configure_default_connections() noexcept {

         connect(&path_button_,&QToolButton::clicked,this,[this]{
                  
                  if(const auto selected_directory = QFileDialog::getExistingDirectory(this);!selected_directory.isEmpty()){
                           path_line_.setText(selected_directory);
                  }
         });

         connect(&cancel_button_,&QPushButton::clicked,this,[this]{
                  reset_lines();
                  reject();
         });

         connect(&download_button_,&QPushButton::clicked,this,&Url_input_dialog::on_input_received);
         connect(&url_line_,&QLineEdit::returnPressed,this,&Url_input_dialog::on_input_received);
         connect(&package_name_line_,&QLineEdit::returnPressed,this,&Url_input_dialog::on_input_received);
         connect(&path_line_,&QLineEdit::returnPressed,this,&Url_input_dialog::on_input_received);
}

void Url_input_dialog::setup_layout() noexcept {
         central_layout_.addLayout(&central_form_layout_);
         central_form_layout_.setSpacing(25);

         central_form_layout_.addRow("URL:",&url_line_);
         central_form_layout_.addRow("Name:",&package_name_line_);
         central_form_layout_.addRow("Path",&path_layout_);
         central_form_layout_.addRow(&button_layout_);

         path_layout_.addWidget(&path_line_);
         path_layout_.addWidget(&path_button_);

         button_layout_.addWidget(&download_button_);
         button_layout_.addWidget(&cancel_button_);
}

void Url_input_dialog::on_input_received() noexcept {
         QUrl url(url_line_.text().simplified());

         if(url.isEmpty()){
                  constexpr std::string_view error_title("Empty URL");
                  constexpr std::string_view error_body("URL field cannot be empty");

                  QMessageBox::critical(this,error_title.data(),error_body.data());
                  return reject();
         }
         
         if(!url.isValid()){
                  static const QString error_body("URL is invalid. Reason: %1");
                  auto error_reason = url.errorString();

                  if(error_reason.isEmpty()){
                           error_reason = "Unknown";
                  }

                  QMessageBox::critical(this,"Invalid URL",error_body.arg(error_reason));
                  return reject();
         }

         auto dir_path = path_line_.text().simplified();

         if(dir_path.isEmpty()){
                  QMessageBox::critical(this,"Invalid Path","Path field cannot be empty");
                  return reject();
         }

         if(dir_path.back() != '/'){
                  dir_path += '/';
         }

         auto package_name = package_name_line_.text().simplified();

         if(package_name.isEmpty()){
                  auto package_name_replacement = url.fileName();

                  if(package_name_replacement.isEmpty()){
                           constexpr std::string_view error_title("Invalid file name");
                           constexpr std::string_view error_body("One of file name field or URL's file name must be non-empty");

                           QMessageBox::critical(this,error_title.data(),error_body.data());
                           return reject();
                  }

                  package_name = std::move(package_name_replacement);
         }

         if(QFileInfo::exists(dir_path + package_name)){
                  constexpr std::string_view query_title("Already exists");
                  constexpr std::string_view query_body("File already exists. Do you wish to replace the existing file?");

                  const auto response_button = QMessageBox::question(this,query_title.data(),query_body.data());

                  if(response_button == QMessageBox::No){
                           return reject();
                  }
         }

         assert(!dir_path.isEmpty());
         assert(!package_name.isEmpty());
         assert(dir_path.back() == '/');

         accept();

         // todo: add more options if temp already exists
         emit new_request_received(dir_path + (url.fileName().isEmpty() ? "temp" : url.fileName()),url);
}