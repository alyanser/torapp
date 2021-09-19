#include "main_window.hxx"
#include "utility.hxx"
#include "torrent_metadata_dialog.hxx"

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

void Main_window::add_top_actions() noexcept {
         auto * const search_action = tool_bar_.addAction("Search");
         auto * const torrent_action = tool_bar_.addAction("Torrent File");
         auto * const url_action = tool_bar_.addAction("Custom Url");
         auto * const exit_action = new QAction("Exit",&file_menu_);

         file_menu_.addAction(search_action);
         file_menu_.addAction(torrent_action);
         file_menu_.addAction(url_action);
         file_menu_.addAction(exit_action);

         search_action->setToolTip("Search for files");
         url_action->setToolTip("Download a file from custom url");
         torrent_action->setToolTip("Download a torrent file");
         exit_action->setToolTip("Exit Torapp");

	connect(torrent_action,&QAction::triggered,[this]{
		constexpr std::string_view caption("Choose a torrent file");
		constexpr std::string_view file_filter("Torrent (*.torrent);; All files (*.*)");
		
		const auto file_path = QFileDialog::getOpenFileName(this,caption.data(),QDir::currentPath(),file_filter.data());

		if(!file_path.isEmpty()){
			Torrent_metadata_dialog torrent_dialog(file_path,this);
			const auto forward_request = qOverload<const bencode::Metadata &>(&Main_window::forward_torrent_download_request);
			connect(&torrent_dialog,&Torrent_metadata_dialog::new_request_received,this,forward_request);
			torrent_dialog.exec();
		}
	});

	connect(url_action,&QAction::triggered,[this]{
		Url_input_widget url_input_widget(this);
		connect(&url_input_widget,&Url_input_widget::new_request_received,this,&Main_window::forward_url_download_request);
		url_input_widget.exec();
	});

         connect(exit_action,&QAction::triggered,this,&Main_window::quit);
}

void Main_window::setup_sort_menu() noexcept {
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
}