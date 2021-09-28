#pragma once

#include "bencode_parser.hxx"

#include <QBigEndianStorageType>
#include <QBitArray>
#include <QUrl>
#include <string_view>

namespace util {

namespace conversion {

enum class Conversion_Format { 
	Speed,
	Memory
};

template<typename byte_type,typename = std::enable_if_t<std::is_arithmetic_v<byte_type>>>
[[nodiscard]]
constexpr std::pair<double,std::string_view> stringify_bytes(const byte_type bytes,const Conversion_Format format) noexcept {
         constexpr auto bytes_in_kb = 1024;
         constexpr auto bytes_in_mb = bytes_in_kb * 1024;
         constexpr auto bytes_in_gb = bytes_in_mb * 1024;

         if(bytes >= bytes_in_gb){
                  return {bytes / bytes_in_gb,format == Conversion_Format::Speed ? "gb (s) /sec" : "gb (s)"};
         }

         if(bytes >= bytes_in_mb){
                  return {bytes / bytes_in_mb,format == Conversion_Format::Speed ? "mb (s) / sec" : "mb (s)"};
         }

         if(bytes >= bytes_in_kb){
                  return {bytes / bytes_in_kb,format == Conversion_Format::Speed ? "kb (s) / sec" : "kb (s)"};
         }

         return {bytes,format == Conversion_Format::Speed ? "byte (s) / sec" : "byte (s)"};
}

template<typename byte_type,typename = std::enable_if_t<std::is_arithmetic_v<byte_type>>>
[[nodiscard]]
QString stringify_bytes(const byte_type bytes_received,const byte_type total_bytes) noexcept {
         double converted_total_bytes = 0;
         std::string_view total_bytes_postfix("inf");

         constexpr auto format = Conversion_Format::Memory;

         if(constexpr auto unknown_bound = -1;total_bytes != unknown_bound){
                  std::tie(converted_total_bytes,total_bytes_postfix) = stringify_bytes(static_cast<double>(total_bytes),format);
         }

         const auto [converted_received_bytes,received_bytes_postfix] = stringify_bytes(static_cast<double>(bytes_received),format);

         QString converted_str("%1 %2 / %3 %4");

         converted_str = converted_str.arg(converted_received_bytes).arg(received_bytes_postfix.data());
         converted_str = converted_str.arg(converted_total_bytes).arg(total_bytes_postfix.data());

         return converted_str;
}

//todo figure SFINAE for 'q.int_.e' types
template<typename numeric_type>
[[nodiscard]]
QByteArray convert_to_hex(const numeric_type num,const std::ptrdiff_t raw_size = sizeof(numeric_type)) noexcept {
	// todo remove after testing
	assert(raw_size == sizeof(numeric_type));

	constexpr auto hex_base = 16;
	const auto hex_size = raw_size * 2;

	auto hex_fmt = QByteArray::number(num,hex_base);

	//todo avoid this
	while(hex_fmt.size() < hex_size){
		hex_fmt.push_front('0');
	}

	return hex_fmt;
}

inline QBitArray convert_to_bits(const QByteArrayView bytes) noexcept {
	constexpr auto bits_in_byte = 8;
	QBitArray bits(bytes.size() * bits_in_byte);

	for(std::ptrdiff_t byte_idx = 0;byte_idx < bytes.size();byte_idx++){

		for(std::ptrdiff_t bit_idx = 0;bit_idx < bits_in_byte;bit_idx++){
			bits.setBit(byte_idx * bits_in_byte + bit_idx,bytes.at(byte_idx) & 1 << bit_idx);
		}
	}

	return bits;
}

inline QByteArray convert_to_hex_bytes(const QBitArray & bits) noexcept {
	constexpr auto bits_in_byte = 8;
	assert(bits.size() % bits_in_byte == 0);
	QByteArray bytes(bits.size() / bits_in_byte,'\x00');

	for(std::ptrdiff_t bit_idx = 0;bit_idx < bits.size();bit_idx++){
		bytes[static_cast<qsizetype>(bit_idx / bits_in_byte)] |= bits[bit_idx] << bit_idx % bits_in_byte;
	}

	return bytes.toHex();
}

} // namespace conversion

template<typename result_type,typename = std::enable_if_t<std::is_arithmetic_v<result_type>>>
[[nodiscard]]
result_type extract_integer(const QByteArray & raw_data,const std::ptrdiff_t offset){
	constexpr auto bytes = static_cast<std::ptrdiff_t>(sizeof(result_type));

	if(offset + bytes > raw_data.size()){
		throw std::out_of_range("extraction out of bounds");
	}

	bool conversion_success = true;
	result_type resultant_integer = 0;
	const auto hex_fmt = raw_data.sliced(offset,bytes).toHex();
	constexpr auto hex_base = 16;

	if constexpr (std::is_same_v<result_type,std::uint32_t>){
		resultant_integer = hex_fmt.toUInt(&conversion_success,hex_base);
	}else if constexpr (std::is_same_v<result_type,std::int32_t>){
		resultant_integer = hex_fmt.toInt(&conversion_success,hex_base);
	}else if constexpr (std::is_same_v<result_type,std::uint64_t>){
		resultant_integer = hex_fmt.toULongLong(&conversion_success,hex_base);
	}else if constexpr (std::is_same_v<result_type,std::int64_t>){
		resultant_integer = hex_fmt.toLongLong(&conversion_success,hex_base);
	}else if constexpr (std::is_same_v<result_type,std::uint16_t>){
		resultant_integer = hex_fmt.toUShort(&conversion_success,hex_base);
	}else if constexpr (std::is_same_v<result_type,std::int16_t>){
		resultant_integer = hex_fmt.toShort(&conversion_success,hex_base);
	}else if constexpr (std::is_same_v<result_type,std::int8_t> || std::is_same_v<result_type,std::uint8_t>){
		resultant_integer = static_cast<result_type>(hex_fmt.toUShort(&conversion_success,hex_base));
	}else{
		throw std::domain_error("invalid type");
	}

	if(!conversion_success){
		throw std::overflow_error("content could not fit in the specified type");
	}

	return resultant_integer;
}

struct Download_request {
         QString package_name;
         QString download_path;
         QUrl url;
};

} // namespace util