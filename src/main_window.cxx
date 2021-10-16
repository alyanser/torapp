#include "main_window.hxx"
#include "torrent_metadata_dialog.hxx"
#include "util.hxx"

#include <bencode_parser.hxx>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>

Main_window::Main_window(){
         setWindowTitle("Torapp");
         setCentralWidget(&central_widget_);
         addToolBar(&tool_bar_);
         setMinimumSize({640,480});

         setup_menu_bar();
         setup_sort_menu();
         add_top_actions();
         read_settings();

         central_layout_.setAlignment(Qt::AlignTop);
}

void Main_window::setup_sort_menu() noexcept {
         auto * const sort_by_name_action = new QAction("By name",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_time_action = new QAction("By time",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_size_action = new QAction("By size",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_progress_action = new QAction("By progress",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_activity_action = new QAction("By activity",&sort_action_group_);

         const auto sort_actions = sort_action_group_.actions();
         assert(!sort_actions.empty());

         for(auto * const sort_action : sort_actions){
                  sort_action->setCheckable(true);
         }

         sort_by_name_action->setChecked(true);
         sort_menu_.addActions(sort_actions);
}

void Main_window::read_settings() noexcept {
         QSettings settings;
         settings.beginGroup("main_window");

         if(settings.contains("size")){
                  resize(settings.value("size").toSize());
                  move(settings.value("pos",QPoint(0,0)).toPoint());
                  show();
         }else{
                  showMaximized();
         }
         
         QTimer::singleShot(0,this,[this]{
                  restore_downloads<QUrl>();
                  restore_downloads<bencode::Metadata>();
         });
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

         connect(exit_action,&QAction::triggered,this,&Main_window::close);

         connect(torrent_action,&QAction::triggered,this,[this]{

                  auto file_path = [this]{
                           constexpr std::string_view file_filter("Torrent (*.torrent);; All files (*.*)");
                           constexpr std::string_view caption("Choose a torrent file");
                           return QFileDialog::getOpenFileName(this,caption.data(),QDir::currentPath(),file_filter.data());
                  }();

                  if(file_path.isEmpty()){
                           return;
                  }
                  
                  Torrent_metadata_dialog torrent_dialog(file_path,this);

                  connect(&torrent_dialog,&Torrent_metadata_dialog::new_request_received,this,[this,file_path = std::move(file_path)](const QString & dl_dir){

                           if(QFile torrent_file(file_path);torrent_file.open(QFile::ReadOnly)){
                                    add_dl_to_settings(dl_dir,torrent_file.readAll());
                           }
                  });

                  connect(&torrent_dialog,&Torrent_metadata_dialog::new_request_received,this,&Main_window::initiate_download<const bencode::Metadata &>);

                  torrent_dialog.exec();
         });

         connect(url_action,&QAction::triggered,this,[this]{
                  Url_input_dialog url_dialog(this);

                  connect(&url_dialog,&Url_input_dialog::new_request_received,this,&Main_window::initiate_download<const QUrl &>);

                  connect(&url_dialog,&Url_input_dialog::new_request_received,this,[this](const QString & file_path,const QUrl url){
                           assert(!file_path.isEmpty());
                           assert(!url.isEmpty());
                           add_dl_to_settings(file_path,url);
                  });

                  url_dialog.exec();
         });
}