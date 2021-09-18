#ifndef BENCODE_PARSER_HXX
#define BENCODE_PARSER_HXX

#include <any>
#include <map>
#include <fstream>
#include <string>
#include <vector>
#include <cassert>
#include <utility>
#include <optional>
#include <iostream>

namespace bencode {

/**
 * @brief Exception class thrown when parsing fails.
 */
class bencode_error : public std::exception {
public:
	using exception::exception;

	explicit bencode_error(const std::string_view error) : what_(error){}

	[[nodiscard]] const char * what() const noexcept override {
		return what_.data();
	}
private:
	std::string_view what_;
};

using result_type = std::map<std::string,std::any>;
using dictionary = result_type;
using list = std::vector<std::any>;

/**
 * @brief Used to specify parsing strictness.
 *
 * Strict : Do not tolerate any error in the given bencoding. Reliable output if any.
 * Relaxed : Tolerate all possible errors. Unrealible output if any.
 */
enum class Parsing_Mode { Strict, Relaxed };

namespace impl {

using list_result = std::optional<std::pair<list,std::size_t>>;
using dictionary_result = std::optional<std::pair<dictionary,std::size_t>>;

template<typename T>
dictionary_result extract_dictionary(T && content,std::size_t content_length,Parsing_Mode mode,std::size_t idx);

template<typename T>
list_result extract_list(T && content,std::size_t content_length,Parsing_Mode mode,std::size_t idx);

} // namespace impl

/**
 * @brief Parses the bencoded file contents and returns decoded keys mapped to corresponding values.
 *
 * @param content Content of the bencoded file.
 * @param parsing_mode Parsing strictness specifier.
 * @return result_type :- [dictionary_titles,values].
 */
template<typename T>
[[nodiscard]] 
result_type parse_content(T && content,const Parsing_Mode parsing_mode = Parsing_Mode::Strict){
	const auto content_length = std::size(content);

	if(const auto dict_opt = impl::extract_dictionary(std::forward<T>(content),content_length,parsing_mode,0)){
		auto & [dict,forward_idx] = dict_opt.value();
		return std::move(dict);
	}

	return {};
}

/**
 * @brief Reads the given bencoded file and passes the conents to bencode::parse_content function.
 * 
 * @param file_path Path of the bencoded file.
 * @param parsing_mode Parsing strictness specifier.
 * @return result_type :- [dictionary_titles,values.
 */
template<typename T>
[[nodiscard]]
result_type parse_file(T && file_path,const Parsing_Mode parsing_mode = Parsing_Mode::Strict){
	std::ifstream in_fstream(std::forward<T>(file_path));

	if(!in_fstream.is_open()){
		throw bencode_error("File doesn't exist or could not be opened for reading");
	}

	std::string content;

	for(std::string temp;std::getline(in_fstream,temp);content += temp){}

	return parse_content(std::move(content),parsing_mode);
}

/**
 * @brief Prints the parsed dictionary recusively.
 * 
 * @param parsed_content Parsed bencoded file contents returned by bencode::parse_file or bencode::parse_content.
 */
inline void dump_content(const std::map<std::string,std::any> & parsed_content) noexcept {

	std::string printable_dict;

	auto dump_list = [](auto compare_hash,const auto & list){
		for(const auto & value : list){
			compare_hash(compare_hash,value,value.type().hash_code());
		}
	};

	auto compare_hash = [dump_list](auto compare_hash,const auto & value,const auto value_type_hash) -> void {
		const static auto label_type_hash = typeid(std::string).hash_code();
		const static auto integer_type_hash = typeid(std::int64_t).hash_code();
		const static auto list_Type_hash = typeid(list).hash_code();
		const static auto dictionary_type_hash = typeid(dictionary).hash_code();

		if(value_type_hash == label_type_hash){
			std::cout << std::any_cast<std::string>(value) << ' ';
		}else if(value_type_hash == integer_type_hash){
			std::cout << std::any_cast<std::int64_t>(value) << ' ';
		}else if(value_type_hash == list_Type_hash){
			dump_list(compare_hash,std::any_cast<list>(value));
		}else if(value_type_hash == dictionary_type_hash){
			dump_content(std::any_cast<dictionary>(value));
		}else{
			__builtin_unreachable();
		}
	};
		
	for(const auto & [dictionary_key,value] : parsed_content){
		std::cout << dictionary_key << "  :  ";

		if(dictionary_key == "pieces"){
			std::cout << "possibly long non-ascii characters (present in dictionary but not being printed)";
		}else{
			compare_hash(compare_hash,value,value.type().hash_code());
		}

		std::cout << '\n';
	}
}

struct Metadata {
	std::vector<std::pair<std::string,std::int64_t>> file_info; // [file_path,file_size]
	std::vector<std::string> announce_list;
	std::string name;
	std::string announce;
	std::string created_by;
	std::string creation_date;
	std::string comment;
	std::string encoding;
	std::string pieces;
	std::int64_t length = 0;
	std::int64_t piece_length = 0;
};

namespace impl {
	void extract_info_dictionary(const dictionary & info_dictionary,Metadata & metadata);
	std::vector<std::string> extract_announce_list(const list & parsed_list);
} // namespace impl

/**
 * @brief Extracts metadata from the parsed contents.
 * 
 * @param parsed_content : Parsed bencoded file contents returned by bencode::parse_file or bencode::parse_content.
 * @return Metadata : Metadata consiting of most common bencode dictionary headers
 */
[[nodiscard]] 
inline Metadata extract_metadata(const dictionary & parsed_content){
	Metadata metadata;
	
	for(const auto & [dict_key,value] : parsed_content){
		if(dict_key == "creation date"){
			metadata.creation_date = std::to_string(std::any_cast<std::int64_t>(value));
		}else if(dict_key == "created by"){
			metadata.created_by = std::any_cast<std::string>(value);
		}else if(dict_key == "encoding"){
			metadata.encoding = std::any_cast<std::string>(value);
		}else if(dict_key == "announce"){
			metadata.announce = std::any_cast<std::string>(value);
		}else if(dict_key == "comment"){
			metadata.comment = std::any_cast<std::string>(value);
		}else if(dict_key == "length"){
			metadata.length = std::any_cast<std::int64_t>(value);
		}else if(dict_key == "announce-list"){
			metadata.announce_list = impl::extract_announce_list(std::any_cast<list>(value));
		}else if(dict_key == "info"){
			impl::extract_info_dictionary(std::any_cast<dictionary>(value),metadata);
		}
	}
	
	return metadata;
}

namespace impl {

using integer_result = std::optional<std::pair<std::int64_t,std::size_t>>;
using label_result = std::optional<std::pair<std::string,std::size_t>>;
using value_result = std::optional<std::pair<std::any,std::size_t>>;

template<typename T>
[[nodiscard]]
integer_result extract_integer(T && content,const std::size_t content_length,const Parsing_Mode parsing_mode,std::size_t idx){
	assert(idx < content_length);

	if(content[idx] != 'i'){
		return {};
	}

	constexpr std::string_view ending_not_found("Ending character ('e') not found for integral value");

	if(++idx == content_length){
		throw bencode_error(ending_not_found.data());
	}

	const bool negative = content[idx] == '-';
	idx += negative;
	std::int64_t result = 0;

	for(;idx < content_length && content[idx] != 'e';++idx){
		if(std::isdigit(content[idx])){
			result *= 10;
			result += content[idx] - '0';
		}else if(parsing_mode == Parsing_Mode::Strict){
			throw bencode_error("Non-digits between 'i' and 'e'");
		}
	}

	if(idx == content_length){
		throw bencode_error(ending_not_found.data());
	}

	assert(content[idx] == 'e');
	return negative ? std::make_pair(-result,idx + 1) : std::make_pair(result,idx + 1);
}

template<typename T>
[[nodiscard]]
label_result extract_label(T && content,const std::size_t content_length,const Parsing_Mode parsing_mode,std::size_t idx){
	assert(idx < content_length);

	if(!std::isdigit(content[idx])){
		return {};
	}

	std::size_t label_length = 0;

	for(;idx < content_length && content[idx] != ':';idx++){
		if(std::isdigit(content[idx])){
			label_length *= 10;
			label_length += static_cast<std::size_t>(content[idx] - '0');
		}else if(parsing_mode == Parsing_Mode::Strict){
			throw bencode_error("Invalid character inside label length");
		}
	}

	if(idx == content_length){
		throw bencode_error("Label separator (:) was not found");
	}

	assert(content[idx] == ':');
	++idx;

	std::string result;
	result.reserve(label_length);

	while(label_length--){
		if(idx >= content_length){
			break;
		}

		result += content[idx++];
	}

	return std::make_pair(std::move(result),idx);
}

template<typename T>
[[nodiscard]]
value_result extract_value(T && content,const std::size_t content_length,const Parsing_Mode parsing_mode,std::size_t idx){

	if(const auto integer_opt = extract_integer(std::forward<T>(content),content_length,parsing_mode,idx)){
		return integer_opt;
	}

	if(const auto label_opt = extract_label(std::forward<T>(content),content_length,parsing_mode,idx)){
		return label_opt;
	}

	if(const auto list_opt = extract_list(std::forward<T>(content),content_length,parsing_mode,idx)){
		return list_opt;
	}

	if(const auto dictionary_opt = extract_dictionary(std::forward<T>(content),content_length,parsing_mode,idx)){
		return dictionary_opt;
	}

	return {};
}

template<typename T>
[[nodiscard]]
list_result extract_list(T && content,const std::size_t content_length,const Parsing_Mode parsing_mode,std::size_t idx){
	assert(idx < content_length);

	if(content[idx] != 'l'){
		return {};
	}

	std::vector<std::any> result;

	for(++idx;idx < content_length;){
		if(content[idx] == 'e'){
			break;
		}

		auto value_opt = extract_value(std::forward<T>(content),content_length,parsing_mode,idx);

		if(value_opt.has_value()){
			auto & [value,forward_idx] = value_opt.value();
			result.emplace_back(std::move(value));
			idx = forward_idx;
		}else if(parsing_mode == Parsing_Mode::Relaxed){
			break;
		}else{
			return {};
		}
	}

	return std::make_pair(std::move(result),idx + 1);
}

template<typename T>
[[nodiscard]]
dictionary_result extract_dictionary(T && content,const std::size_t content_length,const Parsing_Mode parsing_mode,std::size_t idx){
	assert(idx < content_length);

	if(content[idx] != 'd'){
		return {};
	}

	std::map<std::string,std::any> result;

	for(++idx;idx < content_length;){
		if(content[idx] == 'e'){
			break;
		}
		
		const auto key_opt = extract_label(std::forward<T>(content),content_length,parsing_mode,idx);

		if(!key_opt.has_value()){
			if(parsing_mode == Parsing_Mode::Relaxed){
				break;
			}

			throw bencode_error("Invalid or non-existent dictionary key");
		}

		auto & [key,key_forward_idx] = key_opt.value();
		idx = key_forward_idx;

		auto value_opt = extract_value(std::forward<T>(content),content_length,parsing_mode,idx);

		if(value_opt.has_value()){
			auto & [value,forward_idx] = value_opt.value();
			result.emplace(std::move(key),std::move(value));
			idx = forward_idx;
		}else if(parsing_mode == Parsing_Mode::Relaxed){
			break;
		}else{
			return {};
		}
	}

	return std::pair{result,idx + 1};
}

[[nodiscard]]
inline std::vector<std::string> extract_announce_list(const list & parsed_list){
	std::vector<std::string> announce_list;

	announce_list.reserve(parsed_list.size());

	for(const auto & nested_list : parsed_list){
		const auto announce_value = std::any_cast<bencode::list>(nested_list);
		assert(announce_value.size() == 1);
		announce_list.emplace_back(std::any_cast<std::string>(*announce_value.begin()));
	}

	return announce_list;
}

inline void extract_files_info(const list & file_info_list,Metadata & metadata){

	for(const auto & file_info_dict : std::any_cast<list>(file_info_list)){
		metadata.file_info.emplace_back();
		auto & [file_path,file_length] = metadata.file_info.back();

		for(const auto & [file_key,file_value] : std::any_cast<dictionary>(file_info_dict)){
			assert(file_key == "length" || file_key == "path");
			if(file_key == "length"){
				file_length = std::any_cast<std::int64_t>(file_value);
			}else{
				const auto extracted_file_path = std::any_cast<list>(file_value);
				assert(extracted_file_path.size() == 1);
				file_path = std::any_cast<std::string>(extracted_file_path.front());
			}
		}
	}
}

inline void extract_info_dictionary(const dictionary & info_dictionary,Metadata & metadata){

	for(const auto & [info_key,value] : info_dictionary){
		if(info_key == "name"){
			metadata.name = std::any_cast<std::string>(value);
		}else if(info_key == "piece length"){
			metadata.piece_length = std::any_cast<std::int64_t>(value);
		}else if(info_key == "pieces"){
			metadata.pieces = std::any_cast<std::string>(value);
		}else if(info_key == "files"){
			extract_files_info(std::any_cast<list>(value),metadata);
		}else{
			std::cerr << info_key << " not recognized\n";
		}
	}
}

} // namespace impl

} // namespace bencode

#endif // BENCODE_PARSER_HXX