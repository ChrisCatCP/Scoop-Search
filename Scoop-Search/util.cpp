#include "util.h"
#include <memory>
#include <fstream>
#include <iostream>
#include <Windows.h>

namespace util
{
	std::string get_system_bit()
	{
		SYSTEM_INFO si;
		GetNativeSystemInfo(&si);
		if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||
			si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64)
			return "64bit";
		return "32bit";
	}

	std::wstring u2w(const std::string_view& input)
	{
		const size_t wlen = MultiByteToWideChar(CP_UTF8, NULL,
		                                        input.data(), input.size(), nullptr, 0);
		const auto wstr = std::make_shared<wchar_t[]>(wlen);
		MultiByteToWideChar(CP_UTF8,NULL,
		                    input.data(), input.size(), wstr.get(), wlen);
		return {wstr.get(), wlen};
	}

	std::wstring a2w(const std::string_view& input)
	{
		const size_t wlen = MultiByteToWideChar(CP_ACP, NULL,
		                                        input.data(), input.size(), nullptr, 0);
		const auto wstr = std::make_shared<wchar_t[]>(wlen);
		MultiByteToWideChar(CP_ACP, NULL,
		                    input.data(), input.size(), wstr.get(), wlen);
		return {wstr.get(), wlen};
	}

	std::string w2u(const std::wstring_view& input)
	{
		const size_t len = WideCharToMultiByte(CP_UTF8,NULL,
		                                       input.data(), input.size(), nullptr, 0, nullptr, nullptr);
		const auto str = std::make_shared<char[]>(len);
		WideCharToMultiByte(CP_UTF8, NULL,
		                    input.data(), input.size(), str.get(), len, nullptr, nullptr);
		return {str.get(), len};
	}

	std::string w2a(const std::wstring_view& input)
	{
		size_t len = WideCharToMultiByte(CP_ACP, NULL,
		                                 input.data(), input.size(), nullptr, 0, nullptr, nullptr);
		const auto str = std::make_shared<char[]>(len);
		WideCharToMultiByte(CP_ACP, NULL,
		                    input.data(), input.size(), str.get(), len, nullptr, nullptr);
		return {str.get(), len};
	}

	std::string u2a(const std::string_view& input)
	{
		return w2a(u2w(input));
	}

	std::string a2u(const std::string_view& input)
	{
		return w2u(a2w(input));
	}

	std::string read_all(fs::path path)
	{
		std::ifstream input(path);
		return {
			std::istreambuf_iterator(input),
			std::istreambuf_iterator<char>()
		};
	}

	bool string_find(const std::string& str1, const std::string& str2, const bool ignore_case)
	{
		if (ignore_case)
		{
			const auto it = std::ranges::search(str1, str2,
			                                    [](const char ch1, const char ch2)
			                                    {
				                                    return std::toupper(ch1) == std::toupper(ch2);
			                                    }).begin();
			return (it != str1.end());
		}
		return str1.find(str2) != std::string::npos;
	}
}
