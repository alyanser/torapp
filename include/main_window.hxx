#ifndef MAIN_WINDOW_HXX
#define MAIN_WINDOW_HXX

#include "custom_download_widget.hxx"
#include "network_manager.hxx"
#include "download_status_tracker.hxx"

#include <QMainWindow>
#include <QMessageBox>
#include <QToolBar>
#include <string_view>
#include <QMenuBar>
#include <QMenu>
#include <QFile>

class Main_window : public QMainWindow {
         Q_OBJECT
public:
         Main_window();
         Main_window(const Main_window & rhs) = delete;
         Main_window(Main_window && rhs) = delete;
         Main_window & operator = (const Main_window & rhs) = delete;
         Main_window & operator = (Main_window && rhs) = delete;
         ~Main_window() override = default;

private:
         void setup_menu_bar() noexcept;
         void add_top_actions() noexcept;
         
         void input_custom_link() noexcept;
         void confirm_quit() const noexcept;

         QWidget central_widget_;
         QHBoxLayout central_layout_ = QHBoxLayout(&central_widget_);
         QToolBar tool_bar_;
         QMenu file_menu_ = QMenu("File",menuBar());
         Custom_download_widget custom_download_widget_;
         Network_manager network_manager_;
signals:
         void quit() const;
         
public slots:
         void handle_custom_url(const QUrl & custom_url,const QString & download_path) noexcept;
};

inline Main_window::Main_window(){
         setWindowTitle("Torapp");
         setMinimumSize(QSize(800,640));
         setCentralWidget(&central_widget_);
         addToolBar(&tool_bar_);

         tool_bar_.setFloatable(false);

         setup_menu_bar();
         add_top_actions();

         connect(&custom_download_widget_,&Custom_download_widget::request_received,this,&Main_window::handle_custom_url);
}

inline void Main_window::confirm_quit() const noexcept {
         constexpr std::string_view box_title("Quit");
         constexpr std::string_view termination_warning("Are you sure you want to quit? All of your downloads will be terminated.");

         const auto response = QMessageBox::question(nullptr,box_title.data(),termination_warning.data());

         if(response == QMessageBox::Yes){
                  emit quit();
         }
}

inline void Main_window::handle_custom_url(const QUrl & custom_url,const QString & download_path) noexcept {
         auto tracker = std::make_shared<Download_status_tracker>();
         auto file_handle = std::make_shared<QFile>(download_path);

         central_layout_.addWidget(tracker.get());

         if(!custom_url.isValid()){
                  //todo report errors
         }else if(!file_handle->open(QFile::WriteOnly | QFile::Truncate)){
         }else{
                  network_manager_.download(custom_url,tracker,file_handle);
         }
}

#endif // MAIN_WINDOW_HXX