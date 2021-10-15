#pragma once

#include <QBigEndianStorageType>
#include <QBitArray>
#include <QList>

class Download_tracker;
class QFile;

namespace util {

namespace conversion {

enum class Conversion_Format { 
         Speed,
         Memory
};

template<typename byte_type,typename = std::enable_if_t<std::is_arithmetic_v<byte_type>>>
[[nodiscard]]
constexpr std::pair<double,std::string_view> stringify_bytes(const byte_type byte_cnt,const Conversion_Format format) noexcept {
         constexpr byte_type kb_byte_cnt = 1024;
         constexpr byte_type mb_byte_cnt = kb_byte_cnt * 1024;
         constexpr byte_type gb_byte_cnt = mb_byte_cnt * 1024;

         if(byte_cnt >= gb_byte_cnt){
                  return {byte_cnt / gb_byte_cnt,format == Conversion_Format::Speed ? "gb (s) /sec" : "gb (s)"};
         }

         if(byte_cnt >= mb_byte_cnt){
                  return {byte_cnt / mb_byte_cnt,format == Conversion_Format::Speed ? "mb (s) / sec" : "mb (s)"};
         }

         if(byte_cnt >= kb_byte_cnt){
                  return {byte_cnt / kb_byte_cnt,format == Conversion_Format::Speed ? "kb (s) / sec" : "kb (s)"};
         }

         return {byte_cnt,format == Conversion_Format::Speed ? "byte (s) / sec" : "byte (s)"};
}

template<typename byte_type,typename = std::enable_if_t<std::is_arithmetic_v<byte_type>>>
[[nodiscard]]
QString stringify_bytes(const byte_type received_byte_cnt,const byte_type total_byte_cnt) noexcept {
         std::string_view total_bytes_postfix("inf");
         double converted_total_byte_cnt = 0;

         if(constexpr auto unknown_bound = -1;total_byte_cnt != unknown_bound){
                  std::tie(converted_total_byte_cnt,total_bytes_postfix) = stringify_bytes(static_cast<double>(total_byte_cnt),Conversion_Format::Memory);
         }

         const auto [converted_received_byte_cnt,received_bytes_postfix] = stringify_bytes(static_cast<double>(received_byte_cnt),Conversion_Format::Memory);

         auto converted_str = QString("%1 %2 / %3 %4").arg(converted_received_byte_cnt).arg(received_bytes_postfix.data());
         converted_str = converted_str.arg(total_bytes_postfix == "inf" ? "" : QString::number(converted_total_byte_cnt)).arg(total_bytes_postfix.data());
         
         return converted_str;
}

template<typename numeric_type,typename = std::enable_if<std::is_arithmetic_v<numeric_type>>>
numeric_type convert_to_percentile(const numeric_type dividend,const numeric_type divisor) noexcept {
         assert(divisor);
         return static_cast<numeric_type>(static_cast<double>(dividend) / static_cast<double>(divisor) * 100);
}

template<typename numeric_type,typename = std::enable_if_t<std::is_arithmetic_v<numeric_type>>>
[[nodiscard]]
QByteArray convert_to_hex(const numeric_type num,const qsizetype raw_size = static_cast<qsizetype>(sizeof(numeric_type))) noexcept {
         const auto req_hex_size = raw_size * 2;
         constexpr auto hex_base = 16;
         const auto hex_fmt = QByteArray::number(static_cast<QBEInteger<numeric_type>>(num),hex_base);
         assert(req_hex_size - hex_fmt.size() >= 0);
         return QByteArray(req_hex_size - hex_fmt.size(),'0') + hex_fmt;
}

[[nodiscard]]
inline QBitArray convert_to_bits(const QByteArrayView bytes) noexcept {
         constexpr auto bits_in_byte = 8;
         QBitArray bits(bytes.size() * bits_in_byte,false);

         for(qsizetype byte_idx = 0;byte_idx < bytes.size();++byte_idx){

                  for(qsizetype bit_idx = 0;bit_idx < bits_in_byte;++bit_idx){
                           bits.setBit(byte_idx * bits_in_byte + bit_idx,bytes.at(byte_idx) & 1 << (bits_in_byte - 1 - bit_idx));
                  }
         }

         return bits;
}

[[nodiscard]]
inline QByteArray convert_to_bytes(const QBitArray & bits) noexcept {
         constexpr auto bits_in_byte = 8;
         assert(bits.size() % bits_in_byte == 0);
         QByteArray bytes(bits.size() / bits_in_byte,'\x00');

         for(qsizetype bit_idx = 0;bit_idx < bits.size();++bit_idx){
                  bytes[bit_idx / bits_in_byte] |= static_cast<char>(static_cast<qsizetype>(bits[bit_idx]) << (bits_in_byte - 1 - bit_idx % bits_in_byte));
         }

         return bytes.toHex();
}

} // namespace conversion

template<typename result_type,typename = std::enable_if_t<std::is_arithmetic_v<result_type>>>
[[nodiscard]]
result_type extract_integer(const QByteArray & raw_data,const qsizetype offset){
         constexpr auto byte_cnt = static_cast<qsizetype>(sizeof(result_type));

         if(offset + byte_cnt > raw_data.size()){
                  throw std::out_of_range("extraction out of bounds");
         }

         const auto hex_fmt = raw_data.sliced(offset,byte_cnt).toHex();
         constexpr auto hex_base = 16;
         bool conversion_success = true;
         const auto result = static_cast<result_type>(raw_data.sliced(offset,byte_cnt).toHex().toULongLong(&conversion_success,hex_base));

         if(!conversion_success){
                  assert(!result);
                  throw std::overflow_error("content could not fit in the specified type");
         }

         return result;
}

struct Download_resources {
         QString file_path;
         QList<QFile *> file_handles;
         Download_tracker * tracker = nullptr;
};

} // namespace util