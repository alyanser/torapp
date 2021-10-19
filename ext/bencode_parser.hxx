/**
 * Considers this specification as standard : https://wiki.theory.org/BitTorrentSpecification
 */

#ifndef BENCODE_PARSER_HXX
#define BENCODE_PARSER_HXX

#include <optional>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <cassert>
#include <utility>
#include <any>
#include <map>

namespace bencode {

/**
 * @brief Exception class thrown when parsing fails.
 */
class bencode_error : public std::exception {
public:
         using exception::exception;

         explicit bencode_error(const std::string_view error,const std::size_t error_index = 0) 
                  : what_(error)
                  , error_index_(error_index)
         {
         }

         [[nodiscard]] const char * what() const noexcept override {
                  return what_.data();
         }

         [[nodiscard]] std::size_t error_index() const noexcept {
                  return error_index_;
         }
private:
         std::string what_;
         std::size_t error_index_ = 0;
};

using dictionary = std::map<std::string,std::any>;
using list = std::vector<std::any>;
using result_type = dictionary;

/**
 * @brief Can be used to specify parsing strictness.
 *
 * Strict : Do not tolerate any error in the given bencoding. Standard compliant result.
 * Lenient : Tolerate all possible errors. Possibly non-standard result.
 */
enum class Parsing_Mode { Strict, Lenient };

namespace impl {

using list_result = std::optional<std::pair<list,std::size_t>>;
using dictionary_result = std::optional<std::pair<dictionary,std::size_t>>;

template<typename Bencoded>
dictionary_result extract_dictionary(Bencoded && content,std::size_t content_length,Parsing_Mode parsing_mode,std::size_t index);

template<typename Bencoded>
list_result extract_list(Bencoded && content,std::size_t content_length,Parsing_Mode mode,std::size_t index) noexcept;

template<typename Path>
std::string read_file(Path && file_path) noexcept;

} // namespace impl

/**
 * @brief Parses bencoded content and returns decoded keys mapped to corresponding values according to the standard.
 *
 * @param content Bencoded content. Expects it to use standard dictionary keys and other specifications.
 * @param parsing_mode Parsing strictness specifier.
 * @return result_type :- [dictionary_titles,values].
 */
template<typename Bencoded,typename Path>
[[nodiscard]] 
result_type parse_content(Bencoded && content,Path && file_path,const Parsing_Mode parsing_mode = Parsing_Mode::Strict){
         const auto content_length = std::size(content);

         if(!content_length){
                  throw bencode_error("expects non-empty input");
         }

         if(auto dict_opt = impl::extract_dictionary(std::forward<Bencoded>(content),content_length,parsing_mode,0)){
                  auto & [dict,forward_index] = *dict_opt;
                  dict.emplace("file_path",std::string(std::forward<Path>(file_path)));
                  return std::move(dict);
         }

         return {};
}

/**
 * @brief Extracts the content from given bencoded file. Passes the content to becode::parse_content function.
 * 
 * @param file_path Path of the bencoded file.
 * @param parsing_mode Parsing strictness specifier.
 * @return result_type :- [dictionary_titles,values.
 */
template<typename Path>
[[nodiscard]]
result_type parse_file(Path && file_path,const Parsing_Mode parsing_mode = Parsing_Mode::Strict){
         return parse_content(impl::read_file(std::forward<Path>(file_path)),std::forward<Path>(file_path),parsing_mode);
}

namespace impl {
         std::string extract_any(const std::any & value,std::int64_t value_type_hash);
} // namespace impl

/**
 * @brief Extracts the content from parsed dictoinary recursively. Result might contain non-standard dictionary keys.
 * 
 * @param parsed_content : Parsed bencoded file contents returned by bencode::parse_file or bencode::parse_content.
 * @param delimter : Delimter added after each dictionary value.
 * @return std::string : result of conversion
dictionary */
inline std::string convert_to_string(const std::map<std::string,std::any> & parsed_content,const char delimeter = '\n') noexcept {
         std::string dict_content;

         for(const auto & [dictionary_key,value] : parsed_content){
                  dict_content += impl::extract_any(value,static_cast<std::int64_t>(value.type().hash_code()) + delimeter);
         }

         return dict_content;
}

namespace impl {

inline std::string extract_any(const std::any & value,const std::int64_t value_type_hash = static_cast<std::int64_t>(typeid(std::size_t).hash_code())){

         auto extract_inner_list = [](auto && list){
                  std::string list_content;
                  
                  for(const auto & val : list){
                           list_content += extract_any(val,static_cast<std::int64_t>(val.type().hash_code()));
                  }
                  
                  return list_content;
         };

         const static auto label_type_hash = static_cast<std::int64_t>(typeid(std::string).hash_code());
         const static auto integer_type_hash = static_cast<std::int64_t>(typeid(std::int64_t).hash_code());
         const static auto list_type_hash = static_cast<std::int64_t>(typeid(list).hash_code());
         const static auto dict_type_hash = static_cast<std::int64_t>(typeid(dictionary).hash_code());

         if(value_type_hash == label_type_hash){
                  return std::any_cast<std::string>(value);
         }

         if(value_type_hash == integer_type_hash){
                  return std::to_string(std::any_cast<std::int64_t>(value));
         }

         if(value_type_hash == list_type_hash){
                  return extract_inner_list(std::any_cast<list>(value));
         }

         if(value_type_hash == dict_type_hash){
                  return convert_to_string(std::any_cast<dictionary>(value));
         }

         return {};
};

} // namespace impl

/**
 * @brief Metadata containing content of standard-compliaint dictionary keys. Returned from bencode::extract_content
 */
struct Metadata {
         std::string name;
         std::string announce_url;
         std::string created_by;
         std::string creation_date;
         std::string comment;
         std::string encoding;
         std::string pieces;
         std::string md5sum;
         std::string raw_info_dict;
         std::vector<std::pair<std::string,std::int64_t>> file_info; // [file_path,file_size : bytes]
         std::vector<std::string> announce_url_list;
         std::int64_t piece_length = 0;
         std::int64_t single_file_size = 0;
         std::int64_t multiple_files_size = 0;
         bool single_file = true;
};

namespace impl {
         void extract_info_dictionary(const dictionary & info_dictionary,Metadata & metadata) noexcept;
         std::vector<std::string> extract_announce_list(const list & parsed_list) noexcept;
} // namespace impl

/**
 * @brief Pretty convert content of metadata into string. Intended to be used for regex or grep. All dictionary keys
 * guaranteed to be standard-complaint.
 * 
 * @param metadata : Instance returned by bencode::extract_metadata.
 * @param delimeter : Delimeter inserted after each dictionary value.
 * @return std::string : Result of conversion.
 */
[[nodiscard]]
inline std::string convert_to_string(const Metadata & metadata,const std::string_view delimeter = "\n\n") noexcept {
         using namespace std::string_literals;

         std::string str_fmt;

         str_fmt += "- Name : \n\n" + metadata.name + delimeter.data();
         str_fmt += "- Content type : \n\n"s + (metadata.single_file ? "Single file" : "Directory") + delimeter.data();
         str_fmt += "- Total Size \n\n";
         str_fmt += std::to_string((metadata.single_file ? metadata.single_file_size : metadata.multiple_files_size)) + delimeter.data();
         str_fmt += "- Announce URL : \n\n" + metadata.announce_url + delimeter.data();
         str_fmt += "- Created by : \n\n" + metadata.created_by + delimeter.data();
         str_fmt += "- Creation date : \n\n" + metadata.creation_date + delimeter.data();
         str_fmt += "- Comment : \n\n" + metadata.comment + delimeter.data();
         str_fmt += "- Encoding : \n\n" + metadata.encoding + delimeter.data();
         str_fmt += "- Piece length : \n\n" + std::to_string(metadata.piece_length) + delimeter.data();
         str_fmt += "- Announce list : \n\n";

         for(const auto & announce_url : metadata.announce_url_list){
                  str_fmt += announce_url + ' ';
         }

         str_fmt += delimeter.data() + "Files information:\n\n"s;

         for(const auto & [file_path,file_size] : metadata.file_info){
                  str_fmt += "\tPath : " + file_path + "\tSize : " + std::to_string(file_size) + '\n';
         }

         return str_fmt;
}

/**
 * @brief Extracts metadata from the parsed contents. Considers only standard-complaint keys and values.
 * 
 * @param parsed_content : Parsed bencoded file contents returned by bencode::parse_file or bencode::parse_content.
 * @param file_content : No need to provide if bencode::parse_file was called and that same bencoded file exists. Otherwise,
 * can be provided to extract Metadata.raw_info_dict
 */
[[nodiscard]] 
inline Metadata extract_metadata(const dictionary & parsed_content,const std::string & file_content = "") noexcept {
         Metadata metadata;
         
         for(const auto & [dict_key,value] : parsed_content){

                  if(dict_key == "creation date"){
                           metadata.creation_date = std::to_string(std::any_cast<std::int64_t>(value));
                  }else if(dict_key == "created by"){
                           metadata.created_by = std::any_cast<std::string>(value);
                  }else if(dict_key == "encoding"){
                           metadata.encoding = std::any_cast<std::string>(value);
                  }else if(dict_key == "announce"){
                           metadata.announce_url = std::any_cast<std::string>(value);
                  }else if(dict_key == "comment"){
                           metadata.comment = std::any_cast<std::string>(value);
                  }else if(dict_key == "announce-list"){
                           metadata.announce_url_list = impl::extract_announce_list(std::any_cast<list>(value));
                  }else if(dict_key == "info"){
                           impl::extract_info_dictionary(std::any_cast<dictionary>(value),metadata);
                  }else if(dict_key == "info_range"){
                           const auto [info_begin_idx,info_end_idx] = std::any_cast<std::pair<std::size_t,std::size_t>>(value);
                           const auto content = file_content.empty() ? impl::read_file(std::any_cast<std::string>(parsed_content.at("file_path"))) : file_content;

                           if(!content.empty()){
                                    assert(content.size() > info_end_idx);
                                    metadata.raw_info_dict = content.substr(info_begin_idx,info_end_idx - info_begin_idx + 1);
                           }
                  }
         }

         return metadata;
}

namespace impl {

using integer_result = std::optional<std::pair<std::int64_t,std::size_t>>;
using label_result = std::optional<std::pair<std::string,std::size_t>>;
using value_result = std::optional<std::pair<std::any,std::size_t>>;

template<typename Path>
std::string read_file(Path && file_path) noexcept {
         std::ifstream in_stream(std::forward<Path>(file_path));

         if(!in_stream.is_open()){
                  return {};
         }

         std::stringstream s_stream;
         s_stream << in_stream.rdbuf();
         
         return s_stream.str();
}

template<typename Bencoded>
[[nodiscard]]
integer_result extract_integer(Bencoded && content,const std::size_t content_length,const Parsing_Mode parsing_mode,std::size_t index){
         assert(index < content_length);

         if(content[index] != 'i'){
                  return {};
         }

         constexpr std::string_view ending_not_found("Ending character ('e') not found for integral value");

         if(++index == content_length){
                  throw bencode_error(ending_not_found.data(),index);
         }

         const bool negative = content[index] == '-';
         index += negative;
         std::int64_t result = 0;

         for(;index < content_length && content[index] != 'e';++index){

                  if(std::isdigit(content[index])){
                           result *= 10;
                           result += static_cast<std::int64_t>(content[index] - '0');
                  }else if(parsing_mode == Parsing_Mode::Strict){
                           throw bencode_error("Non-digits between 'i' and 'e'",index);
                  }
         }

         if(index == content_length){
                  throw bencode_error(ending_not_found.data(),index);
         }

         assert(content[index] == 'e');

         if(!result && negative){
                  throw bencode_error("Invalid integer value (i-0e)",index);
         }

         return negative ? std::make_pair(-result,index + 1) : std::make_pair(result,index + 1);
}

template<typename Bencoded>
[[nodiscard]]
label_result extract_label(Bencoded && content,const std::size_t content_length,const Parsing_Mode parsing_mode,std::size_t index){
         assert(index < content_length);

         if(!std::isdigit(content[index])){
                  return {};
         }

         std::size_t label_length = 0;

         for(;index < content_length && content[index] != ':';index++){

                  if(std::isdigit(content[index])){
                           label_length *= 10;
                           label_length += static_cast<std::size_t>(content[index] - '0');
                  }else if(parsing_mode == Parsing_Mode::Strict){
                           throw bencode_error("Invalid character inside label length",index);
                  }
         }

         if(index == content_length){
                  throw bencode_error("Label separator (:) was not found",index);
         }

         assert(content[index] == ':');
         ++index;

         std::string result;
         result.reserve(label_length);

         while(index < content_length && label_length--){
                  result += content[index++];
         }

         return std::make_pair(std::move(result),index);
}

template<typename Bencoded>
[[nodiscard]]
value_result extract_value(Bencoded && content,const std::size_t content_length,const Parsing_Mode parsing_mode,std::size_t index) noexcept {

         if(const auto integer_opt = extract_integer(std::forward<Bencoded>(content),content_length,parsing_mode,index)){
                  return integer_opt;
         }

         if(const auto label_opt = extract_label(std::forward<Bencoded>(content),content_length,parsing_mode,index)){
                  return label_opt;
         }

         if(const auto list_opt = extract_list(std::forward<Bencoded>(content),content_length,parsing_mode,index)){
                  return list_opt;
         }

         if(const auto dictionary_opt = extract_dictionary(std::forward<Bencoded>(content),content_length,parsing_mode,index)){
                  return dictionary_opt;
         }

         return {};
}

template<typename Bencoded>
[[nodiscard]]
list_result extract_list(Bencoded && content,const std::size_t content_length,const Parsing_Mode parsing_mode,std::size_t index) noexcept {
         assert(index < content_length);

         if(content[index] != 'l'){
                  return {};
         }

         std::vector<std::any> result;

         for(++index;index < content_length && content[index] != 'e';){
                  auto value_opt = extract_value(std::forward<Bencoded>(content),content_length,parsing_mode,index);

                  if(value_opt){
                           auto & [value,forward_index] = *value_opt;
                           result.emplace_back(std::move(value));
                           index = forward_index;
                  }else if(parsing_mode == Parsing_Mode::Lenient){
                           break;
                  }else{
                           return {};
                  }
         }

         return std::make_pair(std::move(result),index + 1);
}

template<typename Bencoded>
[[nodiscard]]
dictionary_result extract_dictionary(Bencoded && content,const std::size_t content_length,const Parsing_Mode parsing_mode,std::size_t index){
         assert(index < content_length);

         if(content[index] != 'd'){
                  return {};
         }

         std::map<std::string,std::any> result;

         bool is_info_dict = false;

         for(++index;index < content_length && content[index] != 'e';){
                  const auto key_opt = extract_label(std::forward<Bencoded>(content),content_length,parsing_mode,index);

                  if(!key_opt){
                           if(parsing_mode == Parsing_Mode::Lenient){
                                    break;
                           }

                           throw bencode_error("Invalid or non-existent dictionary key",index);
                  }

                  auto & [key,key_forward_index] = *key_opt;
                  index = key_forward_index;

                  if(key == "info"){
                           is_info_dict = true;
                           [[maybe_unused]] const auto [itr,success] = result.emplace("info_range",index);
                           assert(success);
                  }

                  auto value_opt = extract_value(std::forward<Bencoded>(content),content_length,parsing_mode,index);

                  if(value_opt){
                           auto & [value,forward_index] = *value_opt;
                           result.emplace(std::move(key),std::move(value));
                           index = forward_index;
                  }else if(parsing_mode == Parsing_Mode::Lenient){
                           break;
                  }else{
                           return {};
                  }
         }

         if(is_info_dict){
                  auto & value = result["info_range"];
                  const auto info_begin_idx = std::any_cast<std::size_t>(value);
                  assert(info_begin_idx && index);
                  value = std::make_pair(info_begin_idx,index - 1);
         }

         return std::make_pair(std::move(result),index + 1);
}

[[nodiscard]]
inline std::vector<std::string> extract_announce_list(const list & parsed_list) noexcept {
         std::vector<std::string> announce_list;
         announce_list.reserve(parsed_list.size());

         for(const auto & nested_list : parsed_list){
                  const auto announce_value = std::any_cast<bencode::list>(nested_list);
                  assert(announce_value.size() == 1);
                  announce_list.emplace_back(std::any_cast<std::string>(*announce_value.begin()));
         }

         return announce_list;
}

inline void extract_files_info(const list & file_info_list,Metadata & metadata) noexcept {

         for(const auto & file_info_dict : std::any_cast<list>(file_info_list)){
                  metadata.file_info.emplace_back();

                  auto & [file_path,file_length] = metadata.file_info.back();

                  for(const auto & [file_key,file_value] : std::any_cast<dictionary>(file_info_dict)){
                           assert(file_key == "length" || file_key == "path");

                           if(file_key == "length"){
                                    file_length = std::any_cast<std::int64_t>(file_value);
                           }else{
                                    const auto extracted_file_path = std::any_cast<list>(file_value);

                                    for(const auto & file_or_dir : extracted_file_path){
                                             file_path += std::any_cast<std::string>(file_or_dir) + '/';
                                    }

                                    if(!file_path.empty()){
                                             assert(file_path.back() == '/');
                                             file_path.pop_back();
                                    }
                           }
                  }

                  metadata.multiple_files_size += file_length;
         }
}

inline void extract_info_dictionary(const dictionary & info_dictionary,Metadata & metadata) noexcept {

         for(const auto & [info_key,value] : info_dictionary){

                  if(info_key == "name"){
                           metadata.name = std::any_cast<std::string>(value);
                  }else if(info_key == "length"){
                           metadata.single_file_size = std::any_cast<std::int64_t>(value);
                  }else if(info_key == "piece length"){
                           metadata.piece_length = std::any_cast<std::int64_t>(value);
                  }else if(info_key == "pieces"){
                           metadata.pieces = std::any_cast<std::string>(value);
                  }else if(info_key == "md5sum"){
                           metadata.md5sum = std::any_cast<std::string>(value);
                  }else if(info_key == "files"){
                           metadata.single_file = false;
                           extract_files_info(std::any_cast<list>(value),metadata);
                  }
         }
}

} // namespace impl

} // namespace bencode

#endif // BENCODE_PARSER_HXX