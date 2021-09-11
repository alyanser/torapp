#include "main_window.hxx"
#include <QMenuBar>

void Main_window::setup_menu_bar(QMenuBar * const menu_bar) const noexcept {
         auto * const file_menu = new QMenu("File",menu_bar);

         menu_bar->addMenu(file_menu);
         setup_file_menu(file_menu);
}

void Main_window::setup_file_menu(QMenu * const file_menu) const noexcept {
         auto * const custom_link_action = new QAction("Add custom link",file_menu);
         file_menu->addSeparator();
         auto * const exit_action = new QAction("Close",file_menu);

         file_menu->addAction(custom_link_action);
         file_menu->addAction(exit_action);

         connect(custom_link_action,&QAction::triggered,&custom_url_widget_,&Custom_url_widget::show);
         connect(exit_action,&QAction::triggered,this,&Main_window::confirm_quit);
}