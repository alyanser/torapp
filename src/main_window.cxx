#include "main_window.hxx"
#include "torrent_metadata_dialog.hxx"
#include "utility.hxx"

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
         settings_.beginGroup(settings_base_group.data());

         if(settings_.contains("size")){
                  resize(settings_.value("size").toSize());
                  move(settings_.value("pos",QPoint(0,0)).toPoint());
                  show();
         }else{
                  showMaximized();
         }
         
         QTimer::singleShot(0,this,[this]{
                  restore_url_downloads();
                  restore_torrent_downloads();
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

                  connect(&torrent_dialog,&Torrent_metadata_dialog::new_request_received,this,&Main_window::initiate_download<const bencode::Metadata &>);

                  connect(&torrent_dialog,&Torrent_metadata_dialog::new_request_received,this,[this,file_path = std::move(file_path)](const QString & dl_dir){
                           QFile torrent_file(file_path);

                           if(torrent_file.open(QFile::ReadOnly)){
                                    add_dl_to_settings(dl_dir,torrent_file.readAll());
                           }
                  });

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

void Main_window::add_dl_to_settings(const QString & dir_path,const QByteArray & torrent_file_content) noexcept {
         assert(settings_.group() == settings_base_group.data());
         assert(!dir_path.isEmpty());

         settings_.beginGroup("torrent_downloads");
         settings_.beginGroup(QString(dir_path).replace('/','\x20'));

         settings_.setValue("file_path",dir_path);
         settings_.setValue("torrent_file_content",torrent_file_content);

         settings_.endGroup();
         settings_.endGroup(); 
}

void Main_window::add_dl_to_settings(const QString & file_path,const QUrl url) noexcept {
         assert(!file_path.isEmpty());
         assert(settings_.group() == settings_base_group.data());

         settings_.beginGroup("url_downloads");
         settings_.beginGroup(QString(file_path).replace('/','\x20'));

         settings_.setValue("file_path",file_path);
         settings_.setValue("url",url);

         settings_.endGroup();
         settings_.endGroup();
}

void Main_window::remove_dl_from_settings(const QString & parent_group_heading,const QString & file_path) noexcept {
         assert(settings_.group() == settings_base_group.data());

         settings_.beginGroup(parent_group_heading);
         settings_.beginGroup(QString(file_path).replace('/','\x20'));
         
         settings_.remove(""); // removes current group and child keys

         settings_.endGroup();
         settings_.endGroup();
         
         assert(settings_.group() == settings_base_group.data());
         settings_.sync();
}

void Main_window::restore_torrent_downloads() noexcept {
         assert(settings_.group() == settings_base_group.data());
         settings_.beginGroup("torrent_downloads");

         assert(settings_.childKeys().empty());
         const auto torrent_dl_groups = settings_.childGroups();

         for(const auto & torrent_dl_group : torrent_dl_groups){
                  settings_.beginGroup(torrent_dl_group);

                  auto file_path = qvariant_cast<QString>(settings_.value("file_path"));
                  assert(!file_path.isEmpty());

                  auto torrent_file_content = qvariant_cast<QByteArray>(settings_.value("torrent_file_content"));
                  assert(!torrent_file_content.isEmpty());

                  QTimer::singleShot(0,this,[this,file_path = std::move(file_path),file_content = std::move(torrent_file_content)]{

                           const auto torrent_metadata = [&file_path,&file_content]() -> std::optional<bencode::Metadata> {

                                    try{
                                             const auto compl_file_content = file_content.toStdString();
                                             return bencode::extract_metadata(bencode::parse_content(compl_file_content,file_path.toStdString()),compl_file_content);

                                    }catch(const std::exception & exception){
                                             qWarning() << exception.what();
                                             return {};
                                    }
                           }();

                           if(torrent_metadata){
                                    initiate_download(file_path,*torrent_metadata);
                           }
                  });
                  
                  settings_.endGroup();
         }

         settings_.endGroup();
         assert(settings_.group() == settings_base_group.data());
}

void Main_window::restore_url_downloads() noexcept {
         assert(settings_.group() == settings_base_group.data());
         settings_.beginGroup("url_downloads");

         assert(settings_.childKeys().empty());
         const auto url_dl_groups = settings_.childGroups();

         for(const auto & url_dl_group : url_dl_groups){
                  assert(!url_dl_group.isEmpty());

                  settings_.beginGroup(url_dl_group);

                  const auto url = qvariant_cast<QUrl>(settings_.value("url"));
                  assert(!url.isEmpty());

                  auto file_path = qvariant_cast<QString>(settings_.value("file_path"));
                  assert(!file_path.isEmpty());

                  QTimer::singleShot(0,this,[this,file_path = std::move(file_path),url]{
                           initiate_download(file_path,url);
                  });

                  settings_.endGroup();
         }

         settings_.endGroup();
         assert(settings_.group() == settings_base_group.data());
}