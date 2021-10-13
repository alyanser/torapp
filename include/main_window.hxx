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

         template<typename request_type>
         void initiate_download(const QString & path,request_type && download_request) noexcept;
protected:
         void closeEvent(QCloseEvent * event) noexcept override;
private:
         void setup_menu_bar() noexcept;
         void write_settings() noexcept;
         void setup_sort_menu() noexcept;
         void add_top_actions() noexcept;
         void read_settings() noexcept;
         void add_dl_to_settings(const QString & dir_path,const QByteArray & torrent_file_content) noexcept;
         void add_dl_to_settings(const QString & file_path,QUrl url) noexcept;
         void remove_dl_from_settings(const QString & parent_group_heading,const QString & file_path) noexcept;
         void restore_url_downloads() noexcept;
         void restore_torrent_downloads() noexcept;
         ///
         constexpr static std::string_view settings_base_group{"main_window"};
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

template<typename request_type>
void Main_window::initiate_download(const QString & file_path,request_type && download_request) noexcept {
         auto * const tracker = new Download_tracker(file_path,download_request,&central_widget_);
         central_layout_.addWidget(tracker);

         connect(tracker,qOverload<const QString &,request_type>(&Download_tracker::retry_download),this,&Main_window::initiate_download<request_type>);

         auto [file_error,file_handles] = file_manager_.open_file_handles(file_path,download_request);

         connect(tracker,&Download_tracker::download_dropped,this,[this,file_path]{

                  if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<request_type>>,QUrl>){
                           remove_dl_from_settings("url_downloads",file_path);
                  }else{
                           remove_dl_from_settings("torrent_downloads",file_path);
                  }
         });

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
                           
                           network_manager_.download({file_path,std::move(*file_handles),tracker},std::forward<request_type>(download_request));
                           break;
                  }

                  default : {
                           __builtin_unreachable();
                  }
         }
}