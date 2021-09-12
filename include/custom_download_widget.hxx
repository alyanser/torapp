#ifndef CUSTOM_DOWNLOAD_WIDGEt_HXX
#define CUSTOM_DOWNLOAD_WIDGEt_HXX

#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QUrl>

class Custom_download_widget : public QWidget {
         Q_OBJECT
public:
         explicit Custom_download_widget(QWidget * parent = nullptr);
private:
         void on_input_received() noexcept;
         
         QVBoxLayout vertical_layout_ = QVBoxLayout(this);
         QHBoxLayout horizontal_layout_;
         QLineEdit input_line_;
         QPushButton add_button_ = QPushButton("Add");
         QPushButton cancel_button_ = QPushButton("Cancel");

signals:
         void request_received(const QUrl & custom_url) const;
};

inline Custom_download_widget::Custom_download_widget(QWidget * const parent) : QWidget(parent) {
         vertical_layout_.addWidget(&input_line_);
         vertical_layout_.addLayout(&horizontal_layout_);
         horizontal_layout_.addWidget(&add_button_);
         horizontal_layout_.addWidget(&cancel_button_);
         
         input_line_.setPlaceholderText("Paste the link here");

         connect(&input_line_,&QLineEdit::returnPressed,this,&Custom_download_widget::on_input_received);
         
         connect(&add_button_,&QPushButton::clicked,this,&Custom_download_widget::on_input_received);

         connect(&cancel_button_,&QPushButton::clicked,this,[this](){
                  input_line_.clear();
                  hide();
         });
}

inline void Custom_download_widget::on_input_received() noexcept {
         const auto current_url = QUrl(input_line_.text());
         
         input_line_.clear();
         emit request_received(current_url);
         hide();
}

#endif // Custom_download_widget_HXX