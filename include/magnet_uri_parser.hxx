#pragma once

#include <QByteArray>
#include <QList>
#include <QUrl>
#include <optional>
#include <utility>

namespace magnet {

struct Metadata {
         QByteArray display_name;
         QByteArray info_hash;
         QList<QUrl> tracker_urls;
         QList<QUrl> peer_addresses;
         bool is_tagged = false;
};

[[nodiscard]]
inline std::optional<Metadata> parse_magnet_link(const QByteArray & magnet_link) noexcept {
         constexpr auto min_url_size = 60;

         if(magnet_link.size() < min_url_size){
                  return {};
         }

         const auto is_tagged = [&magnet_link]() -> std::optional<bool> {
                  constexpr std::string_view protocol_prefix("magnet:?xt=urn:btih:");
                  constexpr std::string_view tagged_protocol_prefix("magnet:?xt=urn:btmh:");

                  if(magnet_link.startsWith(protocol_prefix.data())){
                           return false;
                  }

                  if(magnet_link.startsWith(tagged_protocol_prefix.data())){
                           return true;
                  }
                  
                  return {};
         }();

         if(!is_tagged.has_value()){
                  return {};
         }
         
         Metadata metadata;
         metadata.is_tagged = *is_tagged;

         metadata.info_hash = [&magnet_link]{
                  constexpr auto info_hash_begin_idx = 20;
                  constexpr auto info_hash_byte_cnt = 40;
                  return magnet_link.sliced(info_hash_begin_idx,info_hash_byte_cnt);
         }();

         constexpr auto delimeter = '&';
         constexpr auto seperator = '=';
         constexpr std::string_view display_name("dn");
         constexpr std::string_view tracker_url("tr");
         constexpr std::string_view peer_address("x.pe");

         for(qsizetype idx = min_url_size + 1;idx < magnet_link.size();){
                  assert(magnet_link[idx] != delimeter && magnet_link[idx] != seperator);

                  const auto seperator_idx = magnet_link.indexOf(seperator,idx);

                  if(seperator_idx == -1){
                           return {};
                  }

                  const auto delimeter_idx = [&magnet_link,seperator_idx]{
                           const auto delimeter_idx = magnet_link.indexOf(delimeter,seperator_idx + 1);
                           return delimeter_idx == -1 ? magnet_link.size() : delimeter_idx;
                  }();

                  const auto header = magnet_link.sliced(idx,seperator_idx - idx);
                  auto content = magnet_link.sliced(seperator_idx + 1,delimeter_idx - seperator_idx - 1);

                  idx = delimeter_idx + 1;

                  if(header == display_name.data()){
                           metadata.display_name = std::move(content);
                  }else if(header == tracker_url.data()){

                           if(auto & url = metadata.tracker_urls.emplace_back(QUrl::fromPercentEncoding(content));!url.isValid()){
                                    return {};
                           }

                  }else if(header == peer_address.data()){
                           constexpr auto ip_seperator = ':';
                           const auto ip_seperator_idx = content.indexOf(ip_seperator);

                           if(ip_seperator_idx == -1){
                                    return {};
                           }

                           auto & url = metadata.peer_addresses.emplace_back();

                           url.setHost(QUrl::fromPercentEncoding(content.sliced(0,ip_seperator_idx)));

                           bool conversion_sucess = false;
                           url.setPort(content.sliced(ip_seperator_idx).toInt(&conversion_sucess));

                           if(!conversion_sucess || !url.isValid()){
                                    return {};
                           }
                  }else{
                           return {};
                  }
         }

         return metadata;
}

} // namespace magnet