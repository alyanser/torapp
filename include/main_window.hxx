#pragma once

#include "url_input_dialog.hxx"
#include "network_manager.hxx"
#include "download_tracker.hxx"
#include "file_manager.hxx"

#include <QActionGroup>
#include <QCloseEvent>
#include <QMessageBox>
#include <QMainWindow>
#include <QSettings>
#include <QToolBar>
#include <QMenuBar>
#include <QDebug>
#include <QMenu>

class Main_window : public QMainWindow {
         Q_OBJECT
public:
         Main_window();
         Main_window(const Main_window & rhs) = delete;
         Main_window(Main_window && rhs) = delete;
         Main_window & operator = (const Main_window & rhs) = delete;
         Main_window & operator = (Main_window && rhs) = delete;
         ~Main_window() override;

         template<typename dl_metadata_type>
         void initiate_download(const QString & path,dl_metadata_type && dl_metadata) noexcept;
protected:
         void closeEvent(QCloseEvent * event) noexcept override;
private:
         template<typename dl_metadata_type>
         void add_dl_to_settings(const QString & path,dl_metadata_type && dl_metadata) noexcept;
         
         template<typename dl_metadata_type>
         void remove_dl_from_settings(const QString & file_path) noexcept;

         template<typename dl_metadata_type>
         void restore_downloads() noexcept;

         template<typename dl_metadata_type>
         void begin_setting_group() noexcept;

         void setup_menu_bar() noexcept;
         void write_settings() noexcept;
         void setup_sort_menu() noexcept;
         void add_top_actions() noexcept;
         void read_settings() noexcept;
         ///
         constexpr static std::string_view base_setting_header{"main_window"};
         constexpr static std::string_view url_dl_setting_header{"url_downloads"};
         constexpr static std::string_view torrent_dl_setting_header{"torrent_downloads"};
         QWidget central_widget_;
         QVBoxLayout central_layout_{&central_widget_};
         QToolBar tool_bar_;
         QMenu file_menu_{"File",menuBar()};
         QMenu sort_menu_{"Sort",menuBar()};
         QActionGroup sort_action_group_{this};
         Network_manager network_manager_;
         File_manager file_manager_;
         QSettings settings_;
};

inline Main_window::~Main_window() {
         write_settings();
}

inline void Main_window::closeEvent(QCloseEvent * const event) noexcept {
         constexpr std::string_view warning_title("Quit");
         constexpr std::string_view warning_body("Are you sure you want to quit? All of the downloads will be stopped.");

         const auto response_button = QMessageBox::question(this,warning_title.data(),warning_body.data());
         response_button == QMessageBox::Yes ? event->accept() : event->ignore();
}

inline void Main_window::setup_menu_bar() noexcept {
         assert(menuBar());
         menuBar()->addMenu(&file_menu_);
         menuBar()->addMenu(&sort_menu_);
}

inline void Main_window::write_settings() noexcept {
         settings_.setValue("size",size());
         settings_.setValue("pos",pos());
}

template<typename dl_metadata_type>
void Main_window::initiate_download(const QString & dl_path,dl_metadata_type && dl_metadata) noexcept {
         auto * const tracker = new Download_tracker(dl_path,dl_metadata,&central_widget_);
         central_layout_.addWidget(tracker);

         connect(tracker,qOverload<decltype(dl_path),dl_metadata_type>(&Download_tracker::retry_download),this,&Main_window::initiate_download<dl_metadata_type>);

         connect(tracker,&Download_tracker::download_dropped,this,[this,dl_path]{
                  remove_dl_from_settings<dl_metadata_type>(dl_path);
         });

         auto [file_error,file_handles] = file_manager_.open_file_handles(dl_path,dl_metadata);

         switch(file_error){
                  
                  case File_manager::File_Error::File_Lock : {
                           // todo: valid desc later
                           [[fallthrough]];
                  }

                  case File_manager::File_Error::Already_Exists : {
                           assert(!file_handles);
                           tracker->set_error_and_finish(Download_tracker::Error::File_Lock);
                           break;
                  }

                  case File_manager::File_Error::Permissions : {
                           assert(!file_handles);
                           tracker->set_error_and_finish(Download_tracker::Error::File_Write);
                           break;
                  }

                  case File_manager::File_Error::Null : {
                           assert(file_handles);
                           assert(!file_handles->isEmpty());
                           assert(tracker->error() == Download_tracker::Error::Null);
                           
                           network_manager_.download({dl_path,std::move(*file_handles),tracker},std::forward<dl_metadata_type>(dl_metadata));
                           break;
                  }

                  default : {
                           __builtin_unreachable();
                  }
         }
}

template<typename dl_metadata_type>
void Main_window::begin_setting_group() noexcept {

         if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<dl_metadata_type>>,QUrl>){
                  settings_.beginGroup(url_dl_setting_header.data());
         }else{
                  settings_.beginGroup(torrent_dl_setting_header.data());
         }
}

template<typename dl_metadata_type>
void Main_window::add_dl_to_settings(const QString & path,dl_metadata_type && dl_metadata) noexcept {
         assert(!path.isEmpty());
         assert(settings_.group() == base_setting_header.data());

         begin_setting_group<dl_metadata_type>();
         settings_.beginGroup(QString(path).replace('/','\x20'));
         settings_.setValue("path",path);
         settings_.setValue("download_metadata",std::forward<dl_metadata_type>(dl_metadata));

         settings_.endGroup();
         settings_.endGroup();

         assert(settings_.group() == base_setting_header.data());
}

template<typename dl_metadata_type>
void Main_window::remove_dl_from_settings(const QString & file_path) noexcept {
         assert(settings_.group() == base_setting_header.data());

         begin_setting_group<dl_metadata_type>();
         settings_.beginGroup(QString(file_path).replace('/','\x20'));
         settings_.remove(""); // removes current group and child keys
         settings_.endGroup();
         settings_.endGroup();

         assert(settings_.group() == base_setting_header.data());
}

template<typename dl_metadata_type>
void Main_window::restore_downloads() noexcept {
         assert(settings_.group() == base_setting_header.data());
         begin_setting_group<dl_metadata_type>();
         assert(settings_.childKeys().empty());

         const auto dl_groups = settings_.childGroups();

         for(const auto & dl_group : settings_.childGroups()){
                  settings_.beginGroup(dl_group);

                  const auto dl_metadata = [this]{

                           if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<dl_metadata_type>>,QUrl>){
                                    return qvariant_cast<QUrl>(settings_.value("download_metadata"));
                           }else{
                                    return qvariant_cast<QByteArray>(settings_.value("download_metadata"));
                           }
                  }();

                  auto path = qvariant_cast<QString>(settings_.value("path"));

                  if(dl_metadata.isEmpty() || path.isEmpty()){
                           constexpr std::string_view error_title("Settings modified");
                           constexpr std::string_view error_body("Torapp settings were modified. Downloads could not be recovered(if any).");
                           QMessageBox::critical(this,error_title.data(),error_body.data());
                           return;
                  }

                  QTimer::singleShot(0,this,[this,path = std::move(path),dl_metadata = std::move(dl_metadata)]{

                           if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<dl_metadata_type>>,QUrl>){
                                    initiate_download(path,dl_metadata);
                           }else{
                                    const auto torrent_metadata = [&path,&dl_metadata]() -> std::optional<bencode::Metadata> {

                                             try{
                                                      const auto compl_file_content = dl_metadata.toStdString();
                                                      return bencode::extract_metadata(bencode::parse_content(compl_file_content,path.toStdString()),compl_file_content);

                                             }catch(const std::exception & exception){
                                                      qDebug() << exception.what();
                                                      return {};
                                             }
                                    }();

                                    if(torrent_metadata){
                                             initiate_download(path,*torrent_metadata);
                                    }
                           }
                  });

                  settings_.endGroup();
         }

         settings_.endGroup();
         assert(settings_.group() == base_setting_header.data());
}