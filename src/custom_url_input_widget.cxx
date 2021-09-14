#include "custom_url_input_widget.hxx"
#include <QFileInfo>

void Custom_url_input_widget::setup_layout() noexcept {
         central_layout_.addLayout(&url_layout_);
         central_layout_.addLayout(&path_layout_);
         central_layout_.addLayout(&button_layout_);

         url_layout_.addWidget(&url_label_);
         url_layout_.addWidget(&url_line_);
         
         button_layout_.addWidget(&download_button_);
         button_layout_.addWidget(&cancel_button_);

         path_layout_.addWidget(&path_label_);
         path_layout_.addWidget(&path_line_);
         path_layout_.addWidget(&path_button_);

         url_label_.setBuddy(&url_line_);
         path_label_.setBuddy(&path_line_);
}

void Custom_url_input_widget::on_input_received() noexcept {
         const auto current_path = path_line_.text().simplified();
         const auto current_url = QUrl(url_line_.text().simplified());

         if(current_path.isEmpty()){
                  return void(QMessageBox::warning(this,"Invalid Path","Path is invalid"));
         }

         if(current_url.toString().isEmpty()){
                  return void(QMessageBox::warning(this,"Invalid Url","Url is invalid"));
         }

         if(QFileInfo::exists(current_path)){
                  constexpr std::string_view query_title("File already exists");
                  constexpr std::string_view query_body("File already exists. Do you wish to replace the existing file?");

                  constexpr auto buttons = QMessageBox::Yes | QMessageBox::No;
                  constexpr auto default_button = QMessageBox::Yes;

                  const auto response_button = QMessageBox::question(this,query_title.data(),query_body.data(),buttons,default_button);

                  if(response_button == QMessageBox::No){
                           return;
                  }
         }
         
         url_line_.clear();
         path_line_.setText(default_path);

         hide();
         emit request_received(current_url,current_path);
}