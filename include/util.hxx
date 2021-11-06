#pragma once

#include <QBigEndianStorageType>
#include <QHashFunctions>
#include <QDateTime>
#include <QSettings>
#include <QBitArray>
#include <QString>
#include <QList>

class Download_tracker;
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
         constexpr bool operator == (const Packet_metadata rhs) const noexcept;
         
         std::int32_t piece_idx = 0;
         std::int32_t piece_offset = 0;
         std::int32_t byte_cnt = 0;
};

[[nodiscard]]
constexpr bool Packet_metadata::operator == (const util::Packet_metadata rhs) const noexcept {
         return byte_cnt == rhs.byte_cnt && piece_idx == rhs.piece_idx && piece_offset == rhs.piece_offset;
}

[[nodiscard]]
constexpr std::size_t qHash(const Packet_metadata packet_metadata,const std::size_t seed = 0) noexcept {
         return ::qHashMulti(seed,1,2,3,packet_metadata.byte_cnt,packet_metadata.piece_idx,packet_metadata.piece_offset);
}

namespace conversion {

enum class Format { 
         Speed,
         Memory
};

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
                  bytes[bit_idx / bits_in_byte] |= static_cast<char>(bits[bit_idx] << (bits_in_byte - 1 - bit_idx % bits_in_byte));
         }

         return bytes.toHex();
}

template<typename numeric_type,typename = std::enable_if_t<std::is_arithmetic_v<numeric_type>>>
[[nodiscard]]
QByteArray convert_to_hex(const numeric_type num) noexcept {
         using unsigned_type = std::make_unsigned_t<numeric_type>;

         constexpr auto hex_base = 16;
         const auto hex_fmt = QByteArray::number(static_cast<QBEInteger<unsigned_type>>(static_cast<unsigned_type>(num)),hex_base);
         assert(!hex_fmt.isEmpty());

         constexpr auto fin_hex_size = static_cast<qsizetype>(sizeof(unsigned_type)) * 2;
         assert(fin_hex_size - hex_fmt.size() >= 0);
         assert(hex_fmt.front() != '-');
         
         return QByteArray(fin_hex_size - hex_fmt.size(),'0') + hex_fmt;
}

template<typename byte_type,typename = std::enable_if_t<std::is_arithmetic_v<byte_type>>>
[[nodiscard]]
constexpr std::pair<double,std::string_view> stringify_bytes(const byte_type byte_cnt,const Format conversion_fmt) noexcept {
         // peer wire protocol use 1000 instead of 1024
         constexpr auto kb_byte_cnt = 1000; 
         constexpr auto mb_byte_cnt = kb_byte_cnt * 1000;
         constexpr auto gb_byte_cnt = mb_byte_cnt * 1000;

         using namespace std::string_view_literals;

         if(byte_cnt >= gb_byte_cnt){
                  return {static_cast<double>(byte_cnt) / gb_byte_cnt,conversion_fmt == Format::Speed ? "gb (s) / sec"sv : "gb (s)"sv};
         }

         if(byte_cnt >= mb_byte_cnt){
                  return {static_cast<double>(byte_cnt) / mb_byte_cnt,conversion_fmt == Format::Speed ? "mb (s) / sec"sv : "mb (s)"sv};
         }

         if(byte_cnt >= kb_byte_cnt){
                  return {static_cast<double>(byte_cnt) / kb_byte_cnt,conversion_fmt == Format::Speed ? "kb (s) / sec"sv : "kb (s)"sv};
         }

         return {byte_cnt,conversion_fmt == Format::Speed ? "byte (s) / sec"sv : "byte (s)"sv};
}

template<typename byte_type,typename = std::enable_if_t<std::is_arithmetic_v<byte_type>>>
[[nodiscard]]
QString stringify_bytes(const byte_type received_byte_cnt,const byte_type total_byte_cnt) noexcept {
         std::string_view total_bytes_postfix("inf");
         double converted_total_byte_cnt = 0;

         if(constexpr auto unknown_bound = -1;total_byte_cnt != unknown_bound){
                  std::tie(converted_total_byte_cnt,total_bytes_postfix) = stringify_bytes(static_cast<double>(total_byte_cnt),Format::Memory);
         }

         const auto [converted_received_byte_cnt,received_bytes_postfix] = stringify_bytes(static_cast<double>(received_byte_cnt),Format::Memory);

         return QString::number(converted_received_byte_cnt,'f',2) + ' ' + received_bytes_postfix.data() + " / " + 
                  (total_bytes_postfix == "inf" ? "inf" : QString::number(converted_total_byte_cnt,'f',2) + ' ' + total_bytes_postfix.data());
}

template<typename numeric_type_x,typename numeric_type_y,typename = std::enable_if_t<std::is_arithmetic_v<std::common_type_t<numeric_type_x,numeric_type_y>>>>
[[nodiscard]]
QString convert_to_percent_format(const numeric_type_x dividend,const numeric_type_y divisor) noexcept {
         assert(divisor);
         return QString::number(static_cast<double>(dividend) / static_cast<double>(divisor) * 100,'f',0) + " %";
}

} // namespace conversion

template<typename result_type,typename = std::enable_if_t<std::is_arithmetic_v<result_type>>>
[[nodiscard]]
result_type extract_integer(const QByteArray & raw_data,const qsizetype offset){
         constexpr auto byte_cnt = static_cast<qsizetype>(sizeof(result_type));

         if(offset + byte_cnt > raw_data.size()){
                  throw std::out_of_range("extraction out of bounds ");
         }

         const auto hex_fmt = raw_data.sliced(offset,byte_cnt).toHex();
         assert(hex_fmt.front() != '-');

         constexpr auto hex_base = 16;
         bool conversion_success = true;
         
         const auto result = static_cast<result_type>(hex_fmt.toULongLong(&conversion_success,hex_base));

         if(!conversion_success){
                  assert(!result);
                  throw std::overflow_error("content could not fit in the specified type");
         }

         return result;
}

template<typename dl_metadata_type>
void begin_setting_group(QSettings & settings) noexcept {

         if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<dl_metadata_type>>,QUrl>){
                  settings.beginGroup("url_downloads");
         }else{
                  settings.beginGroup("torrent_downloads");
         }
}

} // namespace util