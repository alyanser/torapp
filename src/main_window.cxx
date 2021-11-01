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
         setCentralWidget(&scroll_area_);
         addToolBar(&tool_bar_);
         setMinimumSize({640,480});

         assert(menuBar());
         menuBar()->addMenu(&file_menu_);

         add_top_actions();
         read_settings();

         central_layout_.setSpacing(20);
         central_layout_.setAlignment(Qt::AlignTop);
         assert(central_widget_.layout());
         scroll_area_.setWidget(&central_widget_);
         scroll_area_.setWidgetResizable(true);
}

void Main_window::closeEvent(QCloseEvent * const event) noexcept {
         constexpr std::string_view warning_title("Quit");
         constexpr std::string_view warning_body("Are you sure you want to quit? All of the downloads (if any) will be stopped.");
         const auto reply_button = QMessageBox::question(this,warning_title.data(),warning_body.data());
         reply_button == QMessageBox::Yes ? event->accept(),emit closed() : event->ignore();
}

void Main_window::write_settings() const noexcept {
         QSettings settings;
         settings.beginGroup("main_window");
         settings.setValue("size",size());
         settings.setValue("pos",pos());
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
         auto * const torrent_action = tool_bar_.addAction("Torrent");
         auto * const url_action = tool_bar_.addAction("Url");
         auto * const exit_action = new QAction("Exit",&file_menu_);

         file_menu_.addAction(torrent_action);
         file_menu_.addAction(url_action);
         file_menu_.addAction(exit_action);

         torrent_action->setToolTip("Download a torrent file");
         url_action->setToolTip("Download a file from custom url");
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