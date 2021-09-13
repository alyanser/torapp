#ifndef CUSTOM_DOWNLOAD_WIDGET_HXX
#define CUSTOM_DOWNLOAD_WIDGET_HXX

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

class Custom_download_widget : public QWidget {
         Q_OBJECT
public:
         Custom_download_widget();
private:
         void on_input_received() noexcept;

         QString default_path = QDir::currentPath() + "/new_download";
         QVBoxLayout central_layout_ = QVBoxLayout(this);
         QHBoxLayout url_layout_;
         QHBoxLayout path_layout_;
         QHBoxLayout button_layout_;
         
         QLineEdit url_line_;
         QLineEdit path_line_;

         QLabel url_label_ = QLabel("Url: ");
         QLabel path_label_ = QLabel("Path: ");

         QPushButton download_button_ = QPushButton("Download");
         QPushButton cancel_button_ = QPushButton("Cancel");
         QToolButton path_button_;

signals:
         void request_received(const QUrl & custom_url,const QString & path) const;
};

#endif // Custom_download_widget_HXX