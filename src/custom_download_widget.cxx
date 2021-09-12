#include "custom_download_widget.hxx"

Custom_download_widget::Custom_download_widget(QWidget * const parent) : QWidget(parent){
         {
                  constexpr uint32_t min_width = 500;
                  constexpr uint32_t min_height = 500;

                  setMinimumSize(QSize(min_width,min_height));
         }

         setWindowTitle("Custom Download");
         
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
         
         url_line_.setPlaceholderText("Complete url");
         path_line_.setText(default_path);

         connect(&path_button_,&QToolButton::pressed,[this]{
                  const auto selected_directory = QFileDialog::getExistingDirectory(this);

                  if(!selected_directory.isEmpty()){
                           path_line_.setText(selected_directory + "/new_download");
                  }
         });

         connect(&cancel_button_,&QPushButton::clicked,[this]{
                  url_line_.clear();
                  path_line_.setText(default_path);
                  hide();
         });

         connect(&url_line_,&QLineEdit::returnPressed,this,&Custom_download_widget::on_input_received);
         connect(&download_button_,&QPushButton::clicked,this,&Custom_download_widget::on_input_received);
}

void Custom_download_widget::on_input_received() noexcept {
         const auto current_path = path_line_.text();
         const auto current_url = QUrl(url_line_.text());

         if(current_path.isEmpty()){
                  return void(QMessageBox::warning(this,"Empty Path","Path is empty"));
         }

         if(current_url.isEmpty()){
                  return void(QMessageBox::warning(this,"Empty Url","Url is empty"));
         }

         if(QDir().exists(current_path)){
                  constexpr std::string_view warning_title("File already exists");
                  constexpr std::string_view warning_body("File already exists. Do you wish to replace the existing file?");

                  constexpr auto buttons = QMessageBox::Yes | QMessageBox::No;
                  constexpr auto default_button = QMessageBox::Yes;

                  const auto response = QMessageBox::question(this,warning_title.data(),warning_body.data(),buttons,default_button);

                  if(response == QMessageBox::No){
                           return;
                  }
         }
         
         url_line_.clear();
         path_line_.setText(default_path);

         hide();
         emit request_received(current_url,current_path);
}