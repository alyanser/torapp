#pragma once

#include <QByteArray>
#include <QUrl>
#include <QDebug>
#include <vector>
#include <string_view>
#include <optional>

namespace magnet {

struct Metadata {
	QByteArray info_hash;
	QByteArray display_name;
	std::vector<QUrl> tracker_urls;
};

inline std::optional<Metadata> parse(const QByteArray & magnet_url) noexcept {
	constexpr std::string_view required_header("magnet:?xt=urn:btih:");
	constexpr auto hash_size = 40;

	if(magnet_url.size() < static_cast<qsizetype>(hash_size + required_header.size()) || magnet_url.sliced(0, required_header.size()) != required_header.data()) {
		return {};
	}

	Metadata metadata{magnet_url.sliced(required_header.size(), hash_size).toLower(), {"Unknown"}, {}};

	auto extract_string = [&magnet_url](qsizetype & idx, const char delim) {
		QByteArray str;

		for(; idx < magnet_url.size() && magnet_url[idx] != delim; ++idx) {
			str += magnet_url[idx];
		}

		if(idx < magnet_url.size()) {
			++idx; // skip the 'delim'
		}

		return str;
	};

	for(qsizetype idx = required_header.size() + hash_size; idx < magnet_url.size();) {

		if(magnet_url[idx] == '&') {
			++idx;
			continue;
		}

		const auto key = extract_string(idx, '=');
		auto value = extract_string(idx, '&');

		if(key == "dn") {
			metadata.display_name = std::move(value);
		} else if(key == "tr") {
			metadata.tracker_urls.emplace_back(QByteArray::fromPercentEncoding(value));
		} else if(key == "x.pe") {
			// todo: implement direct peer protocol
		} else {
			qDebug() << "magnet url has invalid key" << key << value;
			return {};
		}
	}

	return metadata;
}

} // namespace magnet