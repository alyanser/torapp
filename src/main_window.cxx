#include "main_window.hxx"
#include <QMenuBar>
#include <QAction>

void Main_window::setup_menu_bar() noexcept {
         auto * const menu_bar = menuBar();

         menu_bar->addMenu(&file_menu_);
}

void Main_window::add_top_actions() noexcept {
         auto * const custom_link_action = tool_bar_.addAction("Custom Link");
         auto * const exit_action = tool_bar_.addAction("Close");

         file_menu_.addAction(custom_link_action);
         file_menu_.addAction(exit_action);

         connect(custom_link_action,&QAction::triggered,&custom_download_widget_,&Custom_download_widget::show);
         connect(exit_action,&QAction::triggered,this,&Main_window::confirm_quit);
}