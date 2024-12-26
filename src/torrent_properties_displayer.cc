#include "torrent_properties_displayer.h"
#include "tcp_socket.h"
#include "util.h"

#include <bencode_parser.h>
#include <QDesktopServices>
#include <QProgressBar>
#include <QMessageBox>
#include <QPushButton>
#include <QHeaderView>
#include <QLabel>
#include <QFile>

Torrent_properties_displayer::Torrent_properties_displayer(QWidget * const parent) : QTabWidget(parent) {
	setMinimumSize(200, 200); // totally well thought
	setWindowTitle("Torrent Information");
	setUsesScrollButtons(false);
	setup_peer_table();
}

Torrent_properties_displayer::Torrent_properties_displayer(const bencode::Metadata & torrent_metadata, QWidget * const parent) : Torrent_properties_displayer(parent) {
	addTab(&general_info_tab_, "General");
	addTab(&file_info_scroll_area_, "Files");
	addTab(&peer_table_, "Peers");

	setup_general_info_widget(torrent_metadata);

	file_info_scroll_area_.setWidget(&file_info_tab_);
	file_info_scroll_area_.setWidgetResizable(true);
}

Torrent_properties_displayer::Torrent_properties_displayer(const magnet::Metadata & /* torrent_metadata */, QWidget * const parent) : Torrent_properties_displayer(parent) {
	addTab(&peer_table_, "Peers");
}

void Torrent_properties_displayer::setup_general_info_widget(const bencode::Metadata & torrent_metadata) noexcept {
	general_info_layout_.setAlignment(Qt::AlignCenter);
	general_info_layout_.setSpacing(15);

	auto get_new_label = [&general_info_tab_ = general_info_tab_](const std::string & label_text) {
		auto * const label = new QLabel(label_text.empty() ? "N/A" : QByteArray(label_text.data(), static_cast<qsizetype>(label_text.size())), &general_info_tab_);

		label->setAlignment(Qt::AlignCenter);
		label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		label->setFrameShape(QFrame::Shape::Panel);
		label->setFrameShadow(QFrame::Shadow::Sunken);
		label->setCursor(QCursor(Qt::IBeamCursor));

		return label;
	};

	general_info_layout_.addRow("Name", get_new_label(torrent_metadata.name));
	general_info_layout_.addRow("Created By", get_new_label(torrent_metadata.created_by));
	general_info_layout_.addRow("Creation Time", get_new_label(torrent_metadata.creation_time));
	general_info_layout_.addRow("Comment", get_new_label(torrent_metadata.comment));
	general_info_layout_.addRow("Encoding", get_new_label(torrent_metadata.encoding));
	general_info_layout_.addRow("Md5-Sum", get_new_label(torrent_metadata.md5sum));
	general_info_layout_.addRow("Piece Size:", get_new_label(std::to_string(torrent_metadata.piece_length) + " bytes"));
	general_info_layout_.addRow("Announce URL", get_new_label(torrent_metadata.announce_url));
}

void Torrent_properties_displayer::display_file_bar() noexcept {
	setCurrentWidget(&file_info_tab_);
	show();
}

void Torrent_properties_displayer::remove_peer(std::int32_t peer_row_idx) noexcept {
	assert(peer_row_idx >= 0 && peer_row_idx < peer_table_.rowCount());
	peer_table_.removeRow(peer_row_idx);
}

void Torrent_properties_displayer::setup_peer_table() noexcept {
	peer_table_.horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Stretch);

	const static QList<QString> peer_table_headings{"Peer Id", "Downloaded", "Uploaded", "Download Speed"};

	peer_table_.setColumnCount(static_cast<std::int32_t>(peer_table_headings.size()));
	peer_table_.setHorizontalHeaderLabels(peer_table_headings);
}

QWidget * Torrent_properties_displayer::get_new_file_widget(const QString & file_path, const std::int64_t total_file_size) noexcept {
	auto * const file_widget = new QWidget(&file_info_tab_);
	auto * const file_layout = new QHBoxLayout(file_widget);
	auto * const file_dl_progress_bar = new QProgressBar(&file_info_tab_);
	auto * const open_button = new QPushButton("Open", &file_info_tab_);

	file_layout->addWidget(file_dl_progress_bar);
	file_layout->addWidget(open_button);

	assert(file_dl_progress_bar->parent());
	assert(open_button->parent());

	file_dl_progress_bar->setMaximum(static_cast<std::int32_t>(total_file_size));
	open_button->setEnabled(false);

	connect(file_dl_progress_bar, &QProgressBar::valueChanged, open_button, [file_dl_progress_bar, open_button](const auto new_value) {
		if(new_value == file_dl_progress_bar->maximum()) {
			open_button->setEnabled(true);
		}
	});

	connect(open_button, &QPushButton::clicked, &file_info_tab_, [file_path] {
		if(!QDesktopServices::openUrl(QUrl::fromLocalFile(file_path))) {
			QMessageBox::critical(nullptr, "Could not open", "Could not open the file");
		}
	});

	return file_widget;
}

void Torrent_properties_displayer::setup_file_info_widget(const bencode::Metadata & torrent_metadata, const QList<std::pair<QFile *, std::int64_t>> & file_handles) noexcept {
	assert(file_handles.size() == static_cast<qsizetype>(torrent_metadata.file_info.size()));

	for(qsizetype file_idx = 0; file_idx < file_handles.size(); ++file_idx) {
		const auto & [file_name, file_size] = torrent_metadata.file_info[static_cast<std::size_t>(file_idx)];
		const auto [file_handle, file_dl_byte_cnt] = file_handles[file_idx];
		assert(file_size > 0);
		file_info_layout_.addRow(file_name.data(), get_new_file_widget(file_handle->fileName(), static_cast<std::int64_t>(file_size)));
	}
}

void Torrent_properties_displayer::update_file_info(const qsizetype file_idx, const std::int64_t file_dled_byte_cnt) noexcept {
	assert(file_idx >= 0 && file_idx < file_info_layout_.rowCount());

	auto * const file_dl_progress_bar = [&file_info_layout_ = file_info_layout_, file_idx] {
		auto * const progress_bar_item = file_info_layout_.itemAt(static_cast<std::int32_t>(file_idx), QFormLayout::ItemRole::FieldRole);

		assert(progress_bar_item);
		assert(progress_bar_item->widget());
		assert(progress_bar_item->widget()->layout());
		assert(progress_bar_item->widget()->layout()->count() == 2);

		constexpr auto progress_bar_idx = 0;
		return qobject_cast<QProgressBar *>(progress_bar_item->widget()->layout()->itemAt(progress_bar_idx)->widget());
	}();

	assert(file_dl_progress_bar);
	assert(file_dl_progress_bar->maximum() > 0);

	file_dl_progress_bar->setValue(static_cast<std::int32_t>(file_dled_byte_cnt));
	file_dl_progress_bar->setFormat("Downloaded: " + util::conversion::convert_to_percent_format(file_dled_byte_cnt, file_dl_progress_bar->maximum()));
}

void Torrent_properties_displayer::add_peer(const Tcp_socket * const socket) noexcept {
	assert(socket->state() == Tcp_socket::SocketState::ConnectedState);
	assert(!socket->peer_id.isEmpty());
	assert(peer_table_.columnCount() == 4);

	peer_table_.setRowCount((peer_table_.rowCount() + 1));

	auto get_cell_label_text = [](const auto byte_cnt, const auto conversion_fmt) {
		const auto [converted_byte_cnt, suffix] = util::conversion::stringify_bytes(byte_cnt, conversion_fmt);
		return QString::number(converted_byte_cnt, 'f', 2) + ' ' + suffix.data();
	};

	auto * const dled_byte_cnt_label = [socket, get_cell_label_text] {
		auto * const ret_dled_byte_cnt_label = new QLabel(get_cell_label_text(0, util::conversion::Format::Memory));
		ret_dled_byte_cnt_label->setAlignment(Qt::AlignCenter);

		connect(socket, &Tcp_socket::downloaded_byte_count_changed, ret_dled_byte_cnt_label, [=](const auto dled_byte_cnt) {
			ret_dled_byte_cnt_label->setText(get_cell_label_text(dled_byte_cnt, util::conversion::Format::Memory));
		});

		return ret_dled_byte_cnt_label;
	}();

	auto * const uled_byte_cnt_label = [socket, get_cell_label_text] {
		auto * const ret_uled_byte_cnt_label = new QLabel(get_cell_label_text(0, util::conversion::Format::Memory));
		ret_uled_byte_cnt_label->setAlignment(Qt::AlignCenter);

		connect(socket, &Tcp_socket::uploaded_byte_count_changed, ret_uled_byte_cnt_label, [=](const auto uled_byte_cnt) {
			ret_uled_byte_cnt_label->setText(get_cell_label_text(uled_byte_cnt, util::conversion::Format::Memory));
		});

		return ret_uled_byte_cnt_label;
	}();

	auto * const dl_speed_label = [get_cell_label_text, socket] {
		auto * const ret_dl_speed_label = new QLabel(get_cell_label_text(0, util::conversion::Format::Speed));
		ret_dl_speed_label->setAlignment(Qt::AlignCenter);

		{
			auto * const speed_refresh_timer = new QTimer(ret_dl_speed_label);
			speed_refresh_timer->setInterval(std::chrono::seconds(1));

			speed_refresh_timer->callOnTimeout(socket, [=, seconds_elapsed = 0LL]() mutable {
				ret_dl_speed_label->setText(get_cell_label_text(socket->downloaded_byte_count() / ++seconds_elapsed, util::conversion::Format::Speed));
			});

			speed_refresh_timer->start();
		}

		return ret_dl_speed_label;
	}();

	auto * const peer_id_label = [peer_id = QByteArray::fromHex(socket->peer_id)] {
		assert(peer_id.size() == 20);
		auto * const ret_peer_id_label = new QLabel(peer_id.sliced(0, 8));
		ret_peer_id_label->setAlignment(Qt::AlignCenter);
		return ret_peer_id_label;
	}();

	const auto row_idx = peer_table_.rowCount() - 1;

	enum class Col_idx {
		peer_id,
		dled_byte,
		uled_byte,
		dl_speed
	};

	peer_table_.setCellWidget(row_idx, static_cast<std::int32_t>(Col_idx::peer_id), peer_id_label);
	peer_table_.setCellWidget(row_idx, static_cast<std::int32_t>(Col_idx::dled_byte), dled_byte_cnt_label);
	peer_table_.setCellWidget(row_idx, static_cast<std::int32_t>(Col_idx::uled_byte), uled_byte_cnt_label);
	peer_table_.setCellWidget(row_idx, static_cast<std::int32_t>(Col_idx::dl_speed), dl_speed_label);
}