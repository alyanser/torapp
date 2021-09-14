#include "main_window.hxx"

Main_window::Main_window(){
         {
                  constexpr size_t min_width = 1024;
                  constexpr size_t min_height = 400;

                  setMinimumSize(QSize(min_width,min_height));
         }

         central_layout_.setAlignment(Qt::AlignTop);
         tool_bar_.setFloatable(false);
         
         setWindowTitle("Torapp");
         setCentralWidget(&central_widget_);
         addToolBar(&tool_bar_);
         setup_menu_bar();
         add_top_actions();
         configure_default_connections();
}

void Main_window::initiate_new_download(const QUrl & url,const QString & download_path,const QString & package_name) noexcept {
         assert(!download_path.isEmpty());
         assert(!url.toString().isEmpty());

         auto file_handle = std::make_shared<QFile>(download_path + '/' + package_name);
         auto tracker = std::make_shared<Download_status_tracker>(url,download_path,package_name);

         tracker->bind_lifetime();
         central_layout_.addWidget(tracker.get());

         //! consider the consequences of multiple requests on same file
         if(file_handle->open(QFile::WriteOnly | QFile::Truncate)){
                  network_manager_.download(url,tracker,file_handle);
         }else{
                  tracker->set_error(Download_status_tracker::Error::File_Write);
         }

         connect(tracker.get(),&Download_status_tracker::retry_download,this,&Main_window::initiate_new_download);
}

void Main_window::add_top_actions() noexcept {
         auto * const search_action = tool_bar_.addAction("Search");
         auto * const url_action = tool_bar_.addAction("Custom Url");
         auto * const torrent_action = tool_bar_.addAction("Torrent Url");
         auto * const exit_action = tool_bar_.addAction("Quit");

         file_menu_.addAction(search_action);
         file_menu_.addAction(url_action);
         file_menu_.addAction(torrent_action);
         file_menu_.addAction(exit_action);

         search_action->setToolTip("Search for files");
         url_action->setToolTip("Download a file from custom url");
         torrent_action->setToolTip("Download a torrent file");
         exit_action->setToolTip("Exit Torapp");

         connect(url_action,&QAction::triggered,&custom_download_widget_,&Custom_url_input_widget::show);
         connect(exit_action,&QAction::triggered,this,&Main_window::quit);
}