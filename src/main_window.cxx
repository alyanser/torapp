#include "main_window.hxx"
#include "download_request.hxx"
#include <QLockFile>

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

void Main_window::initiate_new_download(const Download_request & download_request) noexcept {
         assert(!download_request.download_path.isEmpty());
         assert(!download_request.url.toString().isEmpty());

         auto file_handle = std::make_shared<QFile>(download_request.download_path + '/' + download_request.package_name);
         auto file_lock = std::make_shared<QLockFile>(file_handle->fileName());
         auto tracker = std::make_shared<Download_status_tracker>(download_request);

         connect(tracker.get(),&Download_status_tracker::retry_download,this,&Main_window::initiate_new_download);
         connect(&network_manager_,&Network_manager::begin_termination,tracker.get(),&Download_status_tracker::release_lifetime);
         connect(tracker.get(),&Download_status_tracker::destroyed,&network_manager_,&Network_manager::on_tracker_destroyed);

         tracker->bind_lifetime();
         central_layout_.addWidget(tracker.get());
         network_manager_.increment_connection_count();

         if(!file_lock->tryLock()){
                  return void(tracker->set_error_and_finish(Download_status_tracker::Error::File_Lock));
         }

         if(file_handle->open(QFile::WriteOnly | QFile::Truncate)){
                  network_manager_.download(Network_manager::Download_resources{file_handle,file_lock,tracker,download_request.url});
         }else{
                  tracker->set_error_and_finish(Download_status_tracker::Error::File_Write);
         }
}

void Main_window::add_top_actions() noexcept {
         auto * const search_action = tool_bar_.addAction("Search");
         auto * const url_action = tool_bar_.addAction("Custom Url");
         auto * const torrent_action = tool_bar_.addAction("Torrent Url");
         auto * const exit_action = new QAction("Exit",&file_menu_);

         file_menu_.addAction(search_action);
         file_menu_.addAction(url_action);
         file_menu_.addAction(torrent_action);
         file_menu_.addAction(exit_action);

         search_action->setToolTip("Search for files");
         url_action->setToolTip("Download a file from custom url");
         torrent_action->setToolTip("Download a torrent file");
         exit_action->setToolTip("Exit Torapp");

         connect(url_action,&QAction::triggered,&custom_download_widget_,&Custom_url_input_widget::show);
         connect(url_action,&QAction::triggered,&custom_download_widget_,&Custom_url_input_widget::raise);
         connect(exit_action,&QAction::triggered,this,&Main_window::quit);
}