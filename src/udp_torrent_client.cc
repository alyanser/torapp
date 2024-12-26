#include "udp_torrent_client.h"
#include "peer_wire_client.h"
#include "download_tracker.h"
#include "magnet_url_parser.h"

#include <QBigEndianStorageType>
#include <QNetworkDatagram>
#include <QSettings>
#include <QPointer>

Udp_torrent_client::Udp_torrent_client(bencode::Metadata torrent_metadata, util::Download_resources resources, QByteArray info_sha1_hash, QObject * const parent)
    : QObject(parent),
	torrent_metadata_(std::move(torrent_metadata)),
	info_sha1_hash_(info_sha1_hash.isEmpty() ? calculate_info_sha1_hash(torrent_metadata_) : std::move(info_sha1_hash)),
	peer_client_(torrent_metadata_, {resources.dl_path, std::move(resources.file_handles), resources.tracker}, id, info_sha1_hash_),
	tracker_(resources.tracker) {
	configure_default_connections();

	connect(&peer_client_, &Peer_wire_client::existing_pieces_verified, this, [this, dl_path = std::move(resources.dl_path)]() mutable {
		auto restored_dl_paused = [&dl_path] {
			QSettings settings;
			util::begin_setting_group<bencode::Metadata>(settings);
			settings.beginGroup(dl_path.replace('/', '\x20'));
			return qvariant_cast<bool>(settings.value("download_paused", false));
		}();

		if(!restored_dl_paused) {
			send_connect_request();
		} else {
			connect(
			    tracker_, &Download_tracker::download_resumed, this,
			    [this] {
				    send_connect_request();
			    },
			    Qt::SingleShotConnection);
		}
	});
}

Udp_torrent_client::Udp_torrent_client(magnet::Metadata torrent_metadata, util::Download_resources resources, QObject * const parent)
    : QObject(parent),
	info_sha1_hash_(torrent_metadata.info_hash),
	peer_client_(torrent_metadata, {std::move(resources.dl_path), {}, resources.tracker}, id),
	tracker_(resources.tracker) {
	assert(resources.file_handles.isEmpty());
	assert(tracker_);
	configure_default_connections();

	std::ranges::for_each(torrent_metadata.tracker_urls, [this](const auto & tracker_url) {
		assert(tracker_url.isValid());

		auto * const socket = new Udp_socket(tracker_url, craft_connect_request(), this);

		connect(socket, &Udp_socket::readyRead, this, [this, socket] {
			on_socket_ready_read(socket);
		});
	});
}

void Udp_torrent_client::configure_default_connections() noexcept {

	connect(this, &Udp_torrent_client::announce_reply_received, [&peer_client_ = peer_client_, &event_ = event_](const Announce_reply & reply) {
		event_ = Event::Started;

		if(!reply.peer_urls.empty()) {
			peer_client_.connect_to_peers(reply.peer_urls);
		}
	});

	connect(tracker_, &Download_tracker::request_satisfied, this, &Udp_torrent_client::deleteLater);
	connect(&peer_client_, &Peer_wire_client::new_download_requested, this, &Udp_torrent_client::new_download_requested);
}

void Udp_torrent_client::on_socket_ready_read(Udp_socket * const socket) noexcept {

	auto is_valid_socket = [socket = QPointer(socket)] {
		return socket && socket->state() == Udp_socket::SocketState::ConnectedState && socket->hasPendingDatagrams();
	};

	if(!is_valid_socket()) {
		return;
	}

	try {
		communicate_with_tracker(socket);
	} catch(const std::exception & exception) {
		qDebug() << exception.what();
		return socket->abort();
	}

	QTimer::singleShot(0, this, [this, socket, is_valid_socket] {
		if(is_valid_socket()) {
			communicate_with_tracker(socket);
		}
	});
}

void Udp_torrent_client::send_connect_request(const qsizetype tracker_url_idx) noexcept {
	assert(tracker_url_idx >= 0 && tracker_url_idx < static_cast<qsizetype>(torrent_metadata_.announce_url_list.size()));

	if(!connect_requests_sent_) {
		connect_requests_sent_ = true;
	}

	const auto & tracker_url = torrent_metadata_.announce_url_list[static_cast<std::size_t>(tracker_url_idx)];
	auto * const socket = new Udp_socket(QUrl(tracker_url.data()), craft_connect_request(), this);

	connect(socket, &Udp_socket::readyRead, this, [this, socket] {
		on_socket_ready_read(socket);
	});

	if(tracker_url_idx + 1 < static_cast<qsizetype>(torrent_metadata_.announce_url_list.size())) {

		QTimer::singleShot(0, this, [this, tracker_url_idx] {
			send_connect_request(tracker_url_idx + 1);
		});
	}
}

QByteArray Udp_torrent_client::calculate_info_sha1_hash(const bencode::Metadata & torrent_metadata) noexcept {
	const auto raw_info_size = static_cast<qsizetype>(torrent_metadata.raw_info_dict.size());
	return QCryptographicHash::hash(QByteArray(torrent_metadata.raw_info_dict.data(), raw_info_size), QCryptographicHash::Sha1).toHex();
}

bool Udp_torrent_client::verify_txn_id(const QByteArray & reply, const std::int32_t sent_txn_id) {
	constexpr auto txn_id_offset = 4;
	const auto received_txn_id = util::extract_integer<std::int32_t>(reply, txn_id_offset);
	return sent_txn_id == received_txn_id;
}

std::optional<QByteArray> Udp_torrent_client::extract_tracker_error(const QByteArray & reply, const std::int32_t sent_txn_id) {
	constexpr auto error_offset = 8;
	return verify_txn_id(reply, sent_txn_id) ? reply.sliced(error_offset) : std::optional<QByteArray>{};
}

std::optional<std::int64_t> Udp_torrent_client::extract_connect_reply(const QByteArray & reply, const std::int32_t sent_txn_id) {
	constexpr auto connection_id_offset = 8;
	return verify_txn_id(reply, sent_txn_id) ? util::extract_integer<std::int64_t>(reply, connection_id_offset) : std::optional<std::int64_t>{};
}

QByteArray Udp_torrent_client::craft_connect_request() noexcept {
	using util::conversion::convert_to_hex;

	const static auto connect_request = [] {
		constexpr auto protocol_constant = 0x41727101980;
		return convert_to_hex(protocol_constant) + convert_to_hex(static_cast<std::int32_t>(Action_Code::Connect));
	}();

	const auto txn_id = random_id_range(random_generator);
	return connect_request + convert_to_hex(txn_id);
}

QByteArray Udp_torrent_client::craft_announce_request(const std::int64_t tracker_connection_id) const noexcept {
	using util::conversion::convert_to_hex;

	auto announce_request = convert_to_hex(tracker_connection_id);
	constexpr auto fin_announce_request_size = 196;
	announce_request.reserve(fin_announce_request_size);

	announce_request += convert_to_hex(static_cast<std::int32_t>(Action_Code::Announce));

	announce_request += [] {
		const auto txn_id = random_id_range(random_generator);
		return convert_to_hex(txn_id);
	}();

	announce_request += info_sha1_hash_;
	announce_request += id;

	announce_request += convert_to_hex(peer_client_.downloaded_byte_count());
	announce_request += convert_to_hex(peer_client_.remaining_byte_count());
	announce_request += convert_to_hex(peer_client_.uploaded_byte_count());

	announce_request += convert_to_hex(static_cast<std::int32_t>(event_));

	announce_request += [] {
		constexpr auto default_ip_address = 0;
		return convert_to_hex(default_ip_address);
	}();

	announce_request += [] {
		const auto random_peer_key = random_id_range(random_generator);
		return convert_to_hex(random_peer_key);
	}();

	announce_request += [] {
		constexpr auto default_num_want = -1;
		return convert_to_hex(default_num_want);
	}();

	announce_request += [] {
		constexpr std::uint16_t default_port = 6889;
		return convert_to_hex(default_port);
	}();

	assert(announce_request.size() == fin_announce_request_size);
	return announce_request;
}

QByteArray Udp_torrent_client::craft_scrape_request(const std::int64_t tracker_connection_id) const noexcept {
	using util::conversion::convert_to_hex;

	auto scrape_request = convert_to_hex(tracker_connection_id);
	constexpr auto fin_scrape_request_size = 72;
	scrape_request.reserve(fin_scrape_request_size);

	scrape_request += convert_to_hex(static_cast<std::int32_t>(Action_Code::Scrape));

	scrape_request += [] {
		const auto txn_id = random_id_range(random_generator);
		return convert_to_hex(txn_id);
	}();

	scrape_request += info_sha1_hash_;

	assert(scrape_request.size() == fin_scrape_request_size);
	return scrape_request;
}

std::optional<Udp_torrent_client::Announce_reply> Udp_torrent_client::extract_announce_reply(const QByteArray & reply, const std::int32_t sent_txn_id) {

	if(!verify_txn_id(reply, sent_txn_id)) {
		return {};
	}

	const auto interval_time = [&reply] {
		constexpr auto interval_offset = 8;
		return util::extract_integer<std::int32_t>(reply, interval_offset);
	}();

	const auto leecher_cnt = [&reply] {
		constexpr auto leechers_offset = 12;
		return util::extract_integer<std::int32_t>(reply, leechers_offset);
	}();

	const auto seed_cnt = [&reply] {
		constexpr auto seeders_offset = 16;
		return util::extract_integer<std::int32_t>(reply, seeders_offset);
	}();

	auto peer_urls = [&reply] {
		QList<QUrl> peer_urls_ret;

		constexpr auto peers_ip_offset = 20;
		constexpr auto peer_url_byte_cnt = 6;

		for(qsizetype idx = peers_ip_offset; idx < reply.size(); idx += peer_url_byte_cnt) {
			constexpr auto ip_byte_cnt = 4;

			const auto peer_ip = util::extract_integer<std::uint32_t>(reply, idx);
			const auto peer_port = util::extract_integer<std::uint16_t>(reply, idx + ip_byte_cnt);

			QUrl url;
			url.setHost(QHostAddress(peer_ip).toString());
			url.setPort(peer_port);

			if(url.isValid()) {
				peer_urls_ret.emplace_back(std::move(url));
			} else {
				qDebug() << "tracker sent invalid peer url";
			}
		}

		assert(!peer_urls_ret.isEmpty());
		return peer_urls_ret;
	}();

	return Announce_reply{std::move(peer_urls), interval_time, leecher_cnt, seed_cnt};
}

std::optional<Udp_torrent_client::Swarm_metadata> Udp_torrent_client::extract_scrape_reply(const QByteArray & reply, const std::int32_t sent_txn_id) {

	if(!verify_txn_id(reply, sent_txn_id)) {
		return {};
	}

	const auto seed_cnt = [&reply] {
		constexpr auto seed_cnt_offset = 8;
		return util::extract_integer<std::int32_t>(reply, seed_cnt_offset);
	}();

	const auto completed_cnt = [&reply] {
		constexpr auto dl_cnt_offset = 12;
		return util::extract_integer<std::int32_t>(reply, dl_cnt_offset);
	}();

	const auto leecher_cnt = [&reply] {
		constexpr auto leecher_cnt_offset = 16;
		return util::extract_integer<std::int32_t>(reply, leecher_cnt_offset);
	}();

	return Swarm_metadata{seed_cnt, completed_cnt, leecher_cnt};
}

void Udp_torrent_client::communicate_with_tracker(Udp_socket * const socket) {
	assert(socket->state() == Udp_socket::SocketState::ConnectedState);
	assert(socket->hasPendingDatagrams());

	const auto reply = socket->receiveDatagram().data();
	const auto tracker_action = static_cast<Action_Code>(util::extract_integer<std::int32_t>(reply, 0));

	qDebug() << tracker_action << reply.size();

	switch(tracker_action) {

		case Action_Code::Connect: {
			const auto connection_id = extract_connect_reply(reply, socket->transaction_id());

			if(!connection_id) {
				qDebug() << "Invalid connect reply from tracker";
				return socket->abort();
			}

			socket->set_requests(craft_announce_request(*connection_id), craft_scrape_request(*connection_id));
			socket->send_initial_request(socket->announce_request(), Udp_socket::State::Announce);

			auto update_event_and_request = [this, socket = QPointer(socket), connection_id](const Event event) {
				assert(connection_id);

				if(event_ == event) {
					return;
				}

				event_ = event;

				if(!socket) {
					return;
				}

				socket->set_requests(craft_announce_request(*connection_id), craft_scrape_request(*connection_id));

				if(event_ != Event::Stopped) {
					QTimer::singleShot(0, socket, [socket] {
						socket->send_initial_request(socket->announce_request(), Udp_socket::State::Announce);
					});
				}
			};

			connect(tracker_, &Download_tracker::download_resumed, this, [update_event_and_request] {
				update_event_and_request(Event::Started);
			});

			connect(tracker_, &Download_tracker::download_paused, this, [update_event_and_request] {
				update_event_and_request(Event::Stopped);
			});

			connect(&peer_client_, &Peer_wire_client::download_finished, this, [update_event_and_request] {
				update_event_and_request(Event::Completed);
			});

			return;
		}

		case Action_Code::Announce: {

			if(const auto announce_reply = extract_announce_reply(reply, socket->transaction_id())) {
				socket->start_interval_timer(std::chrono::seconds(announce_reply->interval_time));
				emit announce_reply_received(*announce_reply);
			} else {
				qDebug() << "Invalid announce resposne";
				socket->abort();
			}

			return;
		}

		case Action_Code::Scrape: {

			if(const auto scrape_reply = extract_scrape_reply(reply, socket->transaction_id())) {
				emit swarm_metadata_received(*scrape_reply);
			} else {
				qDebug() << "Invalid scrape reply";
				socket->abort();
			}

			return;
		}

		case Action_Code::Error: {

			if(const auto tracker_error = extract_tracker_error(reply, socket->transaction_id())) {
				emit error_received(*tracker_error);
			} else {
				qDebug() << "tracker can't even send the error without errors";
				socket->abort();
			}

			return;
		}

		default: {
			std::unreachable();
		}
	}
}