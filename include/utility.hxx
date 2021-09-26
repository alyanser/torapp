#pragma once

#include "bencode_parser.hxx"

#include <QBigEndianStorageType>
#include <QString>
#include <QUrl>
#include <string_view>
#include <QDebug>

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
QByteArray convert_to_hex(const numeric_type num,const std::ptrdiff_t raw_size) noexcept {
	constexpr auto hex_base = 16;
	const auto hex_size = raw_size * 2;

	auto hex_fmt = QByteArray::number(num,hex_base);

	//todo avoid this
	while(hex_fmt.size() < hex_size){
		hex_fmt.push_front('0');
	}

	return hex_fmt;
}

} // namespace conversion

struct Download_request {
         QString package_name;
         QString download_path;
         QUrl url;
};

} // namespace util