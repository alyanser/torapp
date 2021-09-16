#ifndef MAIN_WINDOW_HXX
#define MAIN_WINDOW_HXX

#include "url_input_widget.hxx"
#include "network_manager.hxx"
#include "download_tracker.hxx"

#include <QMainWindow>
#include <QMessageBox>
#include <QToolBar>
#include <string_view>
#include <QMenuBar>
#include <QMenu>
#include <QFile>
#include <QCloseEvent>
#include <QActionGroup>
#include <QSet>

struct Download_request;

class Main_window : public QMainWindow {
         Q_OBJECT
public:
         Main_window();
         Main_window(const Main_window & rhs) = delete;
         Main_window(Main_window && rhs) = delete;
         Main_window & operator = (const Main_window & rhs) = delete;
         Main_window & operator = (Main_window && rhs) = delete;
         ~Main_window() override = default;
signals:
         void quit() const;
public slots:
         void initiate_new_download(const Download_request & download_request) noexcept;
protected:
         void closeEvent(QCloseEvent * event) noexcept override;
private:
         void configure_default_connections() noexcept;
         void setup_menu_bar() noexcept;
         void setup_sort_menu() noexcept;
         void add_top_actions() noexcept;
         void confirm_quit() const noexcept;
         ///
         QWidget central_widget_;
         QVBoxLayout central_layout_ = QVBoxLayout(&central_widget_);
         QToolBar tool_bar_;
         QMenu file_menu_ = QMenu("File",menuBar());
         QMenu sort_menu_ = QMenu("Sort",menuBar());
         QActionGroup sort_action_group_ = QActionGroup(this);

         Url_input_widget url_input_widget_;
         Network_manager network_manager_;
         QSet<QString> open_files_;
};

inline void Main_window::configure_default_connections() noexcept {
         connect(&network_manager_,&Network_manager::all_trackers_destroyed,this,&Main_window::quit);
         connect(&url_input_widget_,&Url_input_widget::new_request_received,this,&Main_window::initiate_new_download);
}

inline void Main_window::closeEvent(QCloseEvent * const event) noexcept {
         event->ignore();
         confirm_quit();
}

inline void Main_window::confirm_quit() const noexcept {
         constexpr std::string_view warning_title("Quit");
         constexpr std::string_view warning_body("Are you sure you want to quit? All of your downloads will be terminated.");

         const auto response_button = QMessageBox::question(nullptr,warning_title.data(),warning_body.data());

         if(response_button == QMessageBox::Yes){
                  emit network_manager_.terminate();
         }
}

inline void Main_window::setup_menu_bar() noexcept {
         auto * const menu_bar = menuBar();
         
         menu_bar->addMenu(&file_menu_);
         menu_bar->addMenu(&sort_menu_);
}

#endif // MAIN_WINDOW_HXX