#pragma once

#include "utility.hxx"

#include <bencode_parser.hxx>
#include <QMessageBox>
#include <QFileDialog>
#include <QToolButton>
#include <QPushButton>
#include <QFormLayout>
#include <QGridLayout>
#include <QFileInfo>
#include <QLineEdit>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>

class Torrent_metadata_dialog : public QDialog {
         Q_OBJECT
public:
         explicit Torrent_metadata_dialog(const QString & torrent_file_path,QWidget * parent = nullptr);
signals:
         void new_request_received(const QString & dir_path,const bencode::Metadata & metadata) const;
private:
         void extract_metadata(const QString & torrent_file_path) noexcept;
         void setup_layout() noexcept;
         void setup_display(const bencode::Metadata & metadata) noexcept;
         void configure_default_connections() noexcept;
         ///
         QGridLayout central_layout_{this};
         QFormLayout central_form_layout_;

         QLabel torrent_name_label_;
         QLabel created_by_label_;
         QLabel creation_date_label_;
         QLabel comment_label_;
         QLabel encoding_label_;
         QLabel torrent_length_label_;
         QLabel announce_label_;
         QLineEdit path_line_;
         QToolButton path_button_;

         QHBoxLayout button_layout_;
         QPushButton begin_download_button_{"Begin Download"};
         QPushButton cancel_button_{"Cancel"};

         QFormLayout file_form_layout_;
         QVBoxLayout file_layout_;
         QHBoxLayout path_layout_;
};

inline Torrent_metadata_dialog::Torrent_metadata_dialog(const QString & torrent_file_path,QWidget * const parent) 
         : QDialog(parent)
{
         setWindowTitle("Add New Torrent");
         setup_layout();
         extract_metadata(torrent_file_path);
         configure_default_connections();

         path_line_.setText(QFileInfo(torrent_file_path).absolutePath());
}

inline void Torrent_metadata_dialog::configure_default_connections() noexcept {
         connect(&begin_download_button_,&QPushButton::clicked,this,&Torrent_metadata_dialog::accept);
         connect(&cancel_button_,&QPushButton::clicked,this,&Torrent_metadata_dialog::reject);

         connect(&path_button_,&QToolButton::clicked,this,[this]{

                  if(const auto selected_directory = QFileDialog::getExistingDirectory(this);!selected_directory.isEmpty()){
                           path_line_.setText(selected_directory);
                  }
         });
}