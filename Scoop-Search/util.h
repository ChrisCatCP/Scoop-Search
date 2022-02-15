#pragma once

#include <string>
#include <filesystem>

namespace util
{
	namespace fs = std::filesystem;

	std::string get_system_bit();

	std::wstring u2w(const std::string_view& input);
	std::wstring a2w(const std::string_view& input);
	std::string w2u(const std::wstring_view& input);
	std::string w2a(const std::wstring_view& input);
	std::string u2a(const std::string_view& input);
	std::string a2u(const std::string_view& input);

	std::string read_all(fs::path path);

	bool string_find(const std::string& str1, const std::string& str2, bool ignore_case = true);
}