#pragma once

#include "util.hxx"

#include <QFormLayout>
#include <QPushButton>
#include <QToolButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QDialog>
#include <QLabel>

namespace bencode {
         struct Metadata;
}

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
         QLabel torrent_name_label_;
         QLabel created_by_label_;
         QLabel creation_date_label_;
         QLabel comment_label_;
         QLabel encoding_label_;
         QLabel torrent_length_label_;
         QLabel announce_label_;
         QLineEdit path_line_;
         QPushButton begin_download_button_{"Begin Download"};
         QPushButton cancel_button_{"Cancel"};
         QToolButton path_button_;
         QGridLayout central_layout_{this};
         QFormLayout central_form_layout_;
         QFormLayout file_form_layout_;
         QVBoxLayout file_layout_;
         QHBoxLayout path_layout_;
         QHBoxLayout button_layout_;
};