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
#include <QCloseEvent>

class Main_window : public QMainWindow {
         Q_OBJECT
public:
         Main_window();
         Main_window(const Main_window & rhs) = delete;
         Main_window(Main_window && rhs) = delete;
         Main_window & operator = (const Main_window & rhs) = delete;
         Main_window & operator = (Main_window && rhs) = delete;
         ~Main_window() override = default;

protected:
         void closeEvent(QCloseEvent * event) noexcept override;

private:
         void setup_menu_bar() noexcept;
         void add_top_actions() noexcept;
         
         void input_custom_link() noexcept;
         void confirm_quit() const noexcept;

         QWidget central_widget_;
         QVBoxLayout central_layout_ = QVBoxLayout(&central_widget_);
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
         {
                  constexpr size_t min_width = 800;
                  constexpr size_t min_height = 640;

                  setMinimumSize(QSize(min_width,min_height));
         }

         setWindowTitle("Torapp");
         setCentralWidget(&central_widget_);
         addToolBar(&tool_bar_);

         central_layout_.setAlignment(Qt::AlignTop);
         tool_bar_.setFloatable(false);

         setup_menu_bar();
         add_top_actions();

         connect(&network_manager_,&Network_manager::terminated,this,&Main_window::quit);
         connect(&custom_download_widget_,&Custom_download_widget::request_received,this,&Main_window::handle_custom_url);
}

inline void Main_window::closeEvent(QCloseEvent * const event) noexcept {
         event->ignore();
         confirm_quit();
}

inline void Main_window::confirm_quit() const noexcept {
         constexpr std::string_view warning_title("Quit");
         constexpr std::string_view warning_body("Are you sure you want to quit? All of your downloads will be terminated.");

         const auto response = QMessageBox::question(nullptr,warning_title.data(),warning_body.data());

         if(response == QMessageBox::Yes){
                  network_manager_.begin_termination();
         }
}

inline void Main_window::handle_custom_url(const QUrl & custom_url,const QString & download_path) noexcept {
         assert(!download_path.isEmpty());
         assert(!custom_url.toString().isEmpty());

         auto file_handle = std::make_shared<QFile>(download_path);
         auto tracker = std::make_shared<Download_status_tracker>(custom_url.toString(),download_path);

         tracker->bind_lifetime_with_close_button();
         central_layout_.addWidget(tracker.get());

         //! consider the consequences of multiple requests on same file
         if(file_handle->open(QFile::WriteOnly | QFile::Truncate)){
                  network_manager_.download(custom_url,tracker,file_handle);
         }else{
                  tracker->set_misc_state(Download_status_tracker::Misc_State::File_Write_Error);
         }
}

#endif // MAIN_WINDOW_HXX