#pragma once

#include <QHashFunctions>
#include <QString>
#include <QList>
#include <concepts>

class Download_tracker;
class QSettings;
class QFile;

namespace bendode {
struct Metadata;
}

namespace util {

struct Download_resources {
	QString dl_path;
	QList<QFile *> file_handles;
	Download_tracker * tracker = nullptr;
};

struct Packet_metadata {
	std::int32_t piece_idx = 0;
	std::int32_t piece_offset = 0;
	std::int32_t byte_cnt = 0;

	constexpr bool operator==(const Packet_metadata & other) const noexcept = default;
};

constexpr std::size_t qHash(const Packet_metadata packet_metadata, const std::size_t seed = 0) noexcept {
	return ::qHashMulti(seed, packet_metadata.byte_cnt, packet_metadata.piece_idx, packet_metadata.piece_offset);
}

template<typename result_type>
requires std::integral<result_type>
result_type extract_integer(const QByteArray & raw_data, qsizetype offset = 0);

template<typename dl_metadata_type>
void begin_setting_group(QSettings & settings) noexcept;

namespace conversion {

enum class Format {
	Speed,
	Memory
};

QBitArray convert_to_bits(QByteArrayView bytes) noexcept;

QByteArray convert_to_bytes(const QBitArray & bits) noexcept;

template<typename numeric_type>
requires std::integral<numeric_type>
QByteArray convert_to_hex(numeric_type num) noexcept;

template<typename numeric_type_x, typename numeric_type_y>
requires std::integral<std::common_type_t<numeric_type_x, numeric_type_y>>
QString convert_to_percent_format(numeric_type_x dividend, numeric_type_y divisor) noexcept;

template<typename byte_type>
requires std::integral<byte_type> || std::floating_point<byte_type>
QString stringify_bytes(byte_type received_byte_cnt, byte_type total_byte_cnt) noexcept;

template<typename byte_type>
requires std::integral<byte_type> || std::floating_point<byte_type>
constexpr std::pair<double, std::string_view> stringify_bytes(const byte_type byte_cnt, const Format conversion_fmt) noexcept {
	constexpr auto kb_byte_cnt = 1024;
	constexpr auto mb_byte_cnt = kb_byte_cnt * 1024;
	constexpr auto gb_byte_cnt = mb_byte_cnt * 1024;

	using namespace std::string_view_literals;

	if(byte_cnt >= gb_byte_cnt) {
		return {static_cast<double>(byte_cnt) / gb_byte_cnt, conversion_fmt == Format::Speed ? "gb (s) / sec"sv : "gb (s)"sv};
	}

	if(byte_cnt >= mb_byte_cnt) {
		return {static_cast<double>(byte_cnt) / mb_byte_cnt, conversion_fmt == Format::Speed ? "mb (s) / sec"sv : "mb (s)"sv};
	}

	if(byte_cnt >= kb_byte_cnt) {
		return {static_cast<double>(byte_cnt) / kb_byte_cnt, conversion_fmt == Format::Speed ? "kb (s) / sec"sv : "kb (s)"sv};
	}

	return {byte_cnt, conversion_fmt == Format::Speed ? "byte (s) / sec"sv : "byte (s)"sv};
}

} // namespace conversion

} // namespace util