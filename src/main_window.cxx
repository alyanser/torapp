#include "main_window.hxx"
#include "download_request.hxx"

Main_window::Main_window(){
         setMinimumSize(QSize(1024,400));
         setWindowTitle("Torapp");

         setCentralWidget(&central_widget_);
         addToolBar(&tool_bar_);
         
         setup_menu_bar();
         setup_sort_menu();
         add_top_actions();
         configure_default_connections();

         tool_bar_.setFloatable(false);
}

void Main_window::initiate_new_download(const Download_request & download_request) noexcept {
         assert(!download_request.download_path.isEmpty());
         assert(!download_request.url.toString().isEmpty());

         auto file_handle = std::make_shared<QFile>(download_request.download_path + '/' + download_request.package_name);
         auto tracker = std::make_shared<Download_tracker>(download_request);

         tracker->bind_lifetime();
         central_layout_.addWidget(tracker.get());
         network_manager_.increment_connection_count();

         connect(tracker.get(),&Download_tracker::retry_download,this,&Main_window::initiate_new_download);
         connect(&network_manager_,&Network_manager::terminate,tracker.get(),&Download_tracker::release_lifetime);
         connect(tracker.get(),&Download_tracker::destroyed,&network_manager_,&Network_manager::on_tracker_destroyed);

         if(open_files_.contains(file_handle->fileName())){
                  tracker->set_error_and_finish(Download_tracker::Error::File_Lock);
         }else if(!file_handle->open(QFile::WriteOnly | QFile::Truncate)){
                  tracker->set_error_and_finish(Download_tracker::Error::File_Write);
         }else{
                  open_files_.insert(file_handle->fileName());
                  network_manager_.download({file_handle,tracker,download_request.url});

                  connect(file_handle.get(),&QFile::destroyed,[&open_files_ = open_files_,file_name = file_handle->fileName()]{
                           const auto file_itr = open_files_.find(file_name);
                           assert(file_itr != open_files_.end());
                           open_files_.erase(file_itr);
                  });
         }
}

void Main_window::add_top_actions() noexcept {
	//todo switch to icons from text
         auto * const search_action = tool_bar_.addAction("Search");
         auto * const torrent_action = tool_bar_.addAction("Torrent File");
         auto * const url_action = tool_bar_.addAction("Custom Url");
         auto * const exit_action = new QAction("Exit",&file_menu_);

         file_menu_.addAction(search_action);
         file_menu_.addAction(url_action);
         file_menu_.addAction(torrent_action);
         file_menu_.addAction(exit_action);

         search_action->setToolTip("Search for files");
         url_action->setToolTip("Download a file from custom url");
         torrent_action->setToolTip("Download a torrent file");
         exit_action->setToolTip("Exit Torapp");

         connect(url_action,&QAction::triggered,&url_input_widget_,&Url_input_widget::exec);
         connect(exit_action,&QAction::triggered,this,&Main_window::quit);
}

void Main_window::setup_sort_menu() noexcept {
         auto * const sort_by_name_action = new QAction("By name",&sort_action_group_);
         auto * const sort_by_time_action [[maybe_unused]] = new QAction("By time",&sort_action_group_);
         auto * const sort_by_size_action [[maybe_unused]] = new QAction("By size",&sort_action_group_);
         auto * const sort_by_progress_action [[maybe_unused]] = new QAction("By progress",&sort_action_group_);
         auto * const sort_by_activity_action [[maybe_unused]] = new QAction("By activity",&sort_action_group_);
	
         const auto sort_actions = sort_action_group_.actions();

         for(auto * const sort_action : sort_actions){
                  sort_action->setCheckable(true);
         }

         sort_by_name_action->setChecked(true);
         sort_menu_.addActions(sort_actions);

         //todo add connections and implementation
}