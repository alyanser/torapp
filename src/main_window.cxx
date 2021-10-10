#include "main_window.hxx"
#include "torrent_metadata_dialog.hxx"
#include "utility.hxx"

#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <string_view>

Main_window::Main_window(){
         setWindowTitle("Torapp");
         setCentralWidget(&central_widget_);
         addToolBar(&tool_bar_);
         setMinimumSize(QSize(640,480));

         setup_menu_bar();
         setup_sort_menu();
         add_top_actions();

         settings_.beginGroup(setting_header.data());
         
         read_settings();
}

void Main_window::read_settings() noexcept {
         assert(settings_.group() == setting_header.data());

         {
                  settings_.beginGroup("url_downloads");

                  const auto download_groups = settings_.childGroups();

                  for(const auto & download_group : download_groups){
                           settings_.beginGroup(download_group);

                           auto file_path = settings_.value("file_path").value<QString>();
                           const auto url = settings_.value("url").value<QUrl>();

                           QTimer::singleShot(0,this,[this,file_path = std::move(file_path),url]{
                                    initiate_download(file_path,url);
                           });

                           settings_.endGroup();
                  }

                  settings_.endGroup();
         }

         assert(settings_.group() == setting_header.data());
         // todo torrent stuff

         if(settings_.contains("size")){
                  resize(settings_.value("size").toSize());
                  move(settings_.value("pos",QPoint(0,0)).toPoint());
                  show();
         }else{
                  showMaximized();
         }
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
                  constexpr std::string_view caption("Choose a torrent file");
                  constexpr std::string_view file_filter("Torrent (*.torrent);; All files (*.*)");
                  
                  const auto file_path = QFileDialog::getOpenFileName(this,caption.data(),QDir::currentPath(),file_filter.data());

                  if(file_path.isEmpty()){
                           return;
                  }
                  
                  Torrent_metadata_dialog torrent_dialog(file_path,this);
                  connect(&torrent_dialog,&Torrent_metadata_dialog::new_request_received,this,&Main_window::initiate_download<const bencode::Metadata &>);
                  torrent_dialog.exec();
         });

         connect(url_action,&QAction::triggered,this,[this]{
                  Url_input_dialog url_dialog(this);
                  connect(&url_dialog,&Url_input_dialog::new_request_received,this,&Main_window::initiate_download<const QUrl &>);
                  url_dialog.exec();
         });
}

void Main_window::setup_sort_menu() noexcept {
         auto * const sort_by_name_action = new QAction("By name",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_time_action = new QAction("By time",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_size_action = new QAction("By size",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_progress_action = new QAction("By progress",&sort_action_group_);
         [[maybe_unused]] auto * const sort_by_activity_action = new QAction("By activity",&sort_action_group_);

         const auto sort_actions = sort_action_group_.actions();
         assert(!sort_actions.empty());

         // todo: keep the active setting

         for(auto * const sort_action : sort_actions){
                  sort_action->setCheckable(true);
         }

         sort_by_name_action->setChecked(true);
         sort_menu_.addActions(sort_actions);
}

void Main_window::add_dl_metadata_to_settings(const QString & file_path,const bencode::Metadata & torrent_metadata) noexcept {
         assert(settings_.group() == setting_header.data());
         assert(!file_path.isEmpty());

         settings_.beginGroup("torrent_downloads");
         settings_.beginGroup(file_path);

         settings_.setValue("name",QVariant::fromValue(torrent_metadata.name));
         settings_.setValue("announce_url",QVariant::fromValue(torrent_metadata.announce_url));
         settings_.setValue("created_by",QVariant::fromValue(torrent_metadata.created_by));
         settings_.setValue("creation_date",QVariant::fromValue(torrent_metadata.creation_date));
         settings_.setValue("comment",QVariant::fromValue(torrent_metadata.comment));
         settings_.setValue("encoding",QVariant::fromValue(torrent_metadata.encoding));
         settings_.setValue("pieces",QVariant::fromValue(torrent_metadata.pieces));
         settings_.setValue("md5sum",QVariant::fromValue(torrent_metadata.md5sum));
         settings_.setValue("raw_info_dict",QVariant::fromValue(torrent_metadata.raw_info_dict));
         settings_.setValue("file_info",QVariant::fromValue(torrent_metadata.file_info));
         settings_.setValue("announce_url_list",QVariant::fromValue(torrent_metadata.announce_url_list));
         settings_.setValue("piece_length",QVariant::fromValue(torrent_metadata.piece_length));
         settings_.setValue("single_file_size",QVariant::fromValue(torrent_metadata.single_file));
         settings_.setValue("multiple_files_size",QVariant::fromValue(torrent_metadata.multiple_files_size));
         settings_.setValue("single_file",QVariant::fromValue(torrent_metadata.single_file));

         settings_.endGroup();
         settings_.endGroup();
}

void Main_window::add_dl_metadata_to_settings(const QString & file_path,const QUrl url) noexcept {
         assert(!file_path.isEmpty());
         assert(file_path.back() != '/');
         assert(settings_.group() == setting_header.data());

         settings_.beginGroup("url_downloads");

         settings_.beginGroup([&file_path]{
                  const auto last_slash = file_path.lastIndexOf('/');
                  return last_slash == -1 ? file_path : file_path.sliced(last_slash + 1);
         }());

         settings_.setValue("file_path",file_path);
         settings_.setValue("url",url);

         settings_.endGroup();
         settings_.endGroup();
}