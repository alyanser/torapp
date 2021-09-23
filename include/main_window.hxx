#ifndef MAIN_WINDOW_HXX
#define MAIN_WINDOW_HXX

#include "url_input_dialog.hxx"
#include "network_manager.hxx"
#include "download_tracker.hxx"

#include <QActionGroup>
#include <QCloseEvent>
#include <QMessageBox>
#include <QMainWindow>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>

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
	void forward_url_download_request(const util::Download_request & download_request) const;
	void forward_torrent_download_request(const bencode::Metadata & metadata) const;
protected:
         void closeEvent(QCloseEvent * event) noexcept override;
private:
         void configure_default_connections() noexcept;
         void setup_menu_bar() noexcept;
         void setup_sort_menu() noexcept;
         void add_top_actions() noexcept;
         void confirm_quit() noexcept;
         ///
         QWidget central_widget_;
         QVBoxLayout central_layout_ {&central_widget_};
         QToolBar tool_bar_;
         QMenu file_menu_ {"File",menuBar()};
         QMenu sort_menu_ {"Sort",menuBar()};
         QActionGroup sort_action_group_ {this};
         Network_manager network_manager_;
};

inline void Main_window::closeEvent(QCloseEvent * const event) noexcept {
         event->ignore();
         confirm_quit();
}

inline void Main_window::confirm_quit() noexcept {
         constexpr std::string_view warning_title("Quit");
         constexpr std::string_view warning_body("Are you sure you want to quit? All of your downloads will be terminated.");

         const auto response_button = QMessageBox::question(this,warning_title.data(),warning_body.data());

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