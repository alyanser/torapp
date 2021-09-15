#ifndef MAIN_WINDOW_HXX
#define MAIN_WINDOW_HXX

#include "custom_url_input_widget.hxx"
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
#include <QActionGroup>

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

         Custom_url_input_widget custom_download_widget_;
         Network_manager network_manager_;
};

inline void Main_window::configure_default_connections() noexcept {
         connect(&network_manager_,&Network_manager::terminated,this,&Main_window::quit);
         connect(&custom_download_widget_,&Custom_url_input_widget::new_request_received,this,&Main_window::initiate_new_download);
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
                  emit network_manager_.begin_termination();
         }
}

inline void Main_window::setup_menu_bar() noexcept {
         auto * const menu_bar = menuBar();
         menu_bar->addMenu(&file_menu_);
         menu_bar->addMenu(&sort_menu_);
}

inline void Main_window::setup_sort_menu() noexcept {
         auto * const sort_by_name_action = new QAction("By name",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_time_action = new QAction("By time",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_size_action = new QAction("By size",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_progress_action = new QAction("By progress",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_activity_action = new QAction("By activity",&sort_action_group_);

         const auto sort_actions = sort_action_group_.actions();

         for(auto * const sort_action : sort_actions){
                  sort_action->setCheckable(true);
         }

         sort_by_name_action->setChecked(true);
         sort_menu_.addActions(sort_actions);

         //todo add connections and implementation
}

#endif // MAIN_WINDOW_HXX