#include "util.h"

#include <bencode_parser.h>
#include <QBigEndianStorageType>
#include <QSettings>
#include <QBitArray>
#include <tuple>

namespace util {

template<typename result_type>
requires std::integral<result_type>
result_type extract_integer(const QByteArray & raw_data, const qsizetype offset) {
	constexpr auto byte_cnt = static_cast<qsizetype>(sizeof(result_type));

	if(offset + byte_cnt > raw_data.size()) {
		throw std::out_of_range("extraction out of bounds ");
	}

	const auto hex_fmt = raw_data.sliced(offset, byte_cnt).toHex();
	assert(hex_fmt.front() != '-');

	constexpr auto hex_base = 16;
	bool converted = true;

	const auto result = static_cast<result_type>(hex_fmt.toULongLong(&converted, hex_base));

	if(!converted) {
		assert(!result);
		throw std::overflow_error("content could not fit in the specified type");
	}

	return result;
}

template<typename dl_metadata_type>
void begin_setting_group(QSettings & settings) noexcept {

	if constexpr(std::is_same_v<std::remove_cv_t<std::remove_reference_t<dl_metadata_type>>, QUrl>) {
		settings.beginGroup("url_downloads");
	} else {
		settings.beginGroup("torrent_downloads");
	}
}

namespace conversion {

QBitArray convert_to_bits(const QByteArrayView bytes) noexcept {
	constexpr auto bits_in_byte = 8;
	QBitArray bits(bytes.size() * bits_in_byte, false);

	for(qsizetype byte_idx = 0; byte_idx < bytes.size(); ++byte_idx) {

		for(qsizetype bit_idx = 0; bit_idx < bits_in_byte; ++bit_idx) {
			bits.setBit(byte_idx * bits_in_byte + bit_idx, bytes.at(byte_idx) & 1 << (bits_in_byte - 1 - bit_idx));
		}
	}

	return bits;
}

QByteArray convert_to_bytes(const QBitArray & bits) noexcept {
	constexpr auto bits_in_byte = 8;
	assert(bits.size() % bits_in_byte == 0);

	QByteArray bytes(bits.size() / bits_in_byte, '\x00');

	for(qsizetype bit_idx = 0; bit_idx < bits.size(); ++bit_idx) {
		bytes[bit_idx / bits_in_byte] |= static_cast<char>(bits[bit_idx] << (bits_in_byte - 1 - bit_idx % bits_in_byte));
	}

	return bytes.toHex();
}

template<typename numeric_type>
requires std::integral<numeric_type>
QByteArray convert_to_hex(const numeric_type num) noexcept {
	using unsigned_type = std::make_unsigned_t<numeric_type>;

	constexpr auto hex_base = 16;
	const auto hex_fmt = QByteArray::number(static_cast<QBEInteger<unsigned_type>>(static_cast<unsigned_type>(num)), hex_base);
	assert(!hex_fmt.isEmpty());
	assert(hex_fmt.front() != '-');

	constexpr auto fin_hex_size = static_cast<qsizetype>(sizeof(unsigned_type)) * 2;
	assert(fin_hex_size - hex_fmt.size() >= 0);

	return QByteArray(fin_hex_size - hex_fmt.size(), '0') + hex_fmt;
}

template<typename byte_type>
requires std::integral<byte_type> || std::floating_point<byte_type>
QString stringify_bytes(const byte_type received_byte_cnt, const byte_type total_byte_cnt) noexcept {
	std::string_view total_bytes_postfix("inf");
	double converted_total_byte_cnt = 0;

	if(constexpr auto unknown_bound = -1; static_cast<std::make_signed_t<byte_type>>(total_byte_cnt) != unknown_bound) {
		std::tie(converted_total_byte_cnt, total_bytes_postfix) = stringify_bytes(static_cast<double>(total_byte_cnt), Format::Memory);
	}

	const auto [converted_received_byte_cnt, received_bytes_postfix] = stringify_bytes(static_cast<double>(received_byte_cnt), Format::Memory);

	return QString::number(converted_received_byte_cnt, 'f', 2) + ' ' + received_bytes_postfix.data() + " / " +
		 (total_bytes_postfix == "inf" ? "inf" : QString::number(converted_total_byte_cnt, 'f', 2) + ' ' + total_bytes_postfix.data());
}

template<typename numeric_type_x, typename numeric_type_y>
requires std::integral<std::common_type_t<numeric_type_x, numeric_type_y>>
QString convert_to_percent_format(const numeric_type_x dividend, const numeric_type_y divisor) noexcept {
	assert(divisor);
	return QString::number(static_cast<double>(dividend) / static_cast<double>(divisor) * 100, 'f', 0) + " %";
}

template<typename... arith_types>
constexpr auto instantiate_arithmetic_types() {
	static_assert((std::integral<arith_types> && ...));

	return std::tuple_cat(std::make_tuple(&convert_to_hex<arith_types>)..., std::make_tuple(static_cast<QString (*)(arith_types, arith_types)>(&stringify_bytes<arith_types>))...,
				    std::make_tuple(&convert_to_percent_format<arith_types, arith_types>)..., std::make_tuple(&extract_integer<arith_types>)...);
}

extern const auto arithmetic_ins = instantiate_arithmetic_types<std::int32_t, std::uint32_t, std::int16_t, std::uint16_t, std::int64_t, std::uint16_t, std::uint8_t, std::int8_t>();

template QString convert_to_percent_format<std::int64_t, std::int32_t>(std::int64_t, std::int32_t) noexcept;

} // namespace conversion

template void begin_setting_group<QUrl>(QSettings &) noexcept;
template void begin_setting_group<const QUrl &>(QSettings &) noexcept;
template void begin_setting_group<bencode::Metadata>(QSettings &) noexcept;
template void begin_setting_group<const bencode::Metadata &>(QSettings &) noexcept;
template void begin_setting_group<QString>(QSettings &) noexcept;
template void begin_setting_group<const QString &>(QSettings &) noexcept;
template void begin_setting_group<QByteArray>(QSettings &) noexcept;
template void begin_setting_group<const QByteArray &>(QSettings &) noexcept;

} // namespace util