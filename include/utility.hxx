#pragma once

#include "bencode_parser.hxx"

#include <QBigEndianStorageType>
#include <QString>
#include <QUrl>
#include <string_view>

namespace util {

namespace conversion {

enum class Conversion_Format { 
	Speed,
	Memory
};

template<typename Byte_T,typename = std::enable_if_t<std::is_arithmetic_v<Byte_T>>>
[[nodiscard]]
constexpr std::pair<double,std::string_view> stringify_bytes(const Byte_T bytes,const Conversion_Format format) noexcept {
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

template<typename Byte_T,typename = std::enable_if_t<std::is_arithmetic_v<Byte_T>>>
[[nodiscard]]
QString stringify_bytes(const Byte_T bytes_received,const Byte_T total_bytes) noexcept {
         constexpr auto format = Conversion_Format::Memory;
         constexpr auto unknown_bound = -1;

         double converted_total_bytes = 0;
         std::string_view total_bytes_postfix("inf");

         if(total_bytes != unknown_bound){
                  std::tie(converted_total_bytes,total_bytes_postfix) = stringify_bytes(static_cast<double>(total_bytes),format);
         }

         const auto [converted_received_bytes,received_bytes_postfix] = stringify_bytes(static_cast<double>(bytes_received),format);

         QString converted_str("%1 %2 / %3 %4");

         converted_str = converted_str.arg(converted_received_bytes).arg(received_bytes_postfix.data());
         converted_str = converted_str.arg(converted_total_bytes).arg(total_bytes_postfix.data());

         return converted_str;
}

//todo figure SFINAE for 'q.int_.e' types
template<typename Numeric_T>
[[nodiscard]]
QByteArray convert_to_hex(const Numeric_T number,const std::ptrdiff_t size_required) noexcept {
	constexpr auto hex_base = 16;

	auto hex_fmt = QByteArray::fromHex(QByteArray::number(number,hex_base));

	while(hex_fmt.size() < size_required){
		hex_fmt.push_front('\x00');
	}

	return hex_fmt.size() == size_required ? hex_fmt : QByteArray{};
}

} // namespace conversion

struct Download_request {
         QString package_name;
         QString download_path;
         QUrl url;
};

} // namespace util