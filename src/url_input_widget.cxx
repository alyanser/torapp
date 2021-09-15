#include "url_input_widget.hxx"
#include "download_request.hxx"

#include <QFileInfo>

void Url_input_widget::setup_layout() noexcept {
         central_layout_.addLayout(&url_layout_);
         central_layout_.addLayout(&package_name_layout_);
         central_layout_.addLayout(&path_layout_);
         central_layout_.addLayout(&button_layout_);

         url_layout_.addWidget(&url_label_);
         url_layout_.addWidget(&url_line_);
         
         button_layout_.addWidget(&download_button_);
         button_layout_.addWidget(&cancel_button_);

         path_layout_.addWidget(&path_label_);
         path_layout_.addWidget(&path_line_);
         path_layout_.addWidget(&path_button_);

         package_name_layout_.addWidget(&package_name_label_);
         package_name_layout_.addWidget(&package_name_line_);
         package_name_label_.setBuddy(&package_name_line_);

         url_label_.setBuddy(&url_line_);
         path_label_.setBuddy(&path_line_);
}

void Url_input_widget::on_input_received() noexcept {
         const QUrl url(url_line_.text().simplified());

         if(url.isEmpty()){
                  constexpr std::string_view error_title("Empty URL");
                  constexpr std::string_view error_body("URL field cannot be empty");

                  return void(QMessageBox::critical(this,error_title.data(),error_body.data()));
         }
         
         if(!url.isValid()){
                  const QString error_body("URL is invalid. Reason: %1");
                  auto error_reason = url.errorString();

                  if(error_reason.isEmpty()){
                           error_reason = "Unknown";
                  }

                  constexpr std::string_view error_title("Invalid URL");

                  return void(QMessageBox::critical(this,error_title.data(),error_body.arg(error_reason)));
         }

         auto path = path_line_.text().simplified();

         if(path.isEmpty()){
                  return void(QMessageBox::critical(this,"Invalid Path","Path field cannot be empty"));
         }

         if(path.back() != '/'){
                  path += '/';
         }

         auto package_name = package_name_line_.text().simplified();

         if(package_name.isEmpty()){
                  auto name_replacement = url.fileName();

                  if(name_replacement.isEmpty()){
                           constexpr std::string_view error_title("Invalid file name");
                           constexpr std::string_view error_body("One of file name field or URL's file name must be non-empty");
                           
                           return void(QMessageBox::critical(this,error_title.data(),error_body.data()));
                  }

                  package_name = std::move(name_replacement);
         }

         if(QFileInfo::exists(path + package_name)){
                  constexpr std::string_view query_title("Already exists");
                  constexpr std::string_view query_body("File already exists. Do you wish to replace the existing file?");
         
                  constexpr auto buttons = QMessageBox::Yes | QMessageBox::No;
                  constexpr auto default_button = QMessageBox::Yes;

                  const auto response_button = QMessageBox::question(this,query_title.data(),query_body.data(),buttons,default_button);

                  if(response_button == QMessageBox::No){
                           return;
                  }
         }
         
         reset_lines();
         hide();
         emit new_request_received({package_name,path,url});
}