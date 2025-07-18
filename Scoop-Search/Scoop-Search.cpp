#include "Scoop-Search.h"
#include "thread_pool.hpp"
#include "console_color.h"
#include "util.h"
#include "json.hpp"
#include <string>
#include <filesystem>
#include <fstream>
#include <format>
#include <regex>
#include <Windows.h>


namespace fs = std::filesystem;

namespace scoop
{
	struct app_info
	{
		std::string name;
		std::string version;
		std::string description;
		std::vector<std::string> binaries;
		std::vector<std::string> shortcuts;
	};

	using match_result_t = std::optional<app_info>;
	using search_result_t = std::vector<std::future<match_result_t>>;


	static thread_pool worker{max(std::thread::hardware_concurrency() / 2, 1)};
	static std::string system_bit = util::get_system_bit();

	static std::optional<fs::path> find_scoop_by_env()
	{
		const auto scoop_env = getenv("SCOOP");
		if (!scoop_env) return std::nullopt;
		fs::path scoop_path = scoop_env;
		if (scoop_path.empty())
			return std::nullopt;
		if (!exists(scoop_path / "shims" / "scoop.ps1"))
			return std::nullopt;
		return std::move(scoop_path);
	}

	static std::optional<fs::path> find_scoop_by_path()
	{
		const auto size = SearchPathW(nullptr, L"scoop.ps1", nullptr, 0, nullptr, nullptr);
		if (!size) return std::nullopt;
		const auto buffer = std::make_unique<wchar_t[]>(size);
		SearchPathW(nullptr, L"scoop.ps1", nullptr, size, buffer.get(), nullptr);
		const fs::path scoop = buffer.get();
		if (scoop.parent_path().stem() != L"shims")
			return std::nullopt;
		return scoop.parent_path().parent_path();
	}

	static fs::path get_scoop_path()
	{
		auto scoop_path = find_scoop_by_env();
		if (scoop_path) return scoop_path.value();
		scoop_path = find_scoop_by_path();
		if (scoop_path) return scoop_path.value();
		std::cerr << "error:cant find scoop path" << '\n';
		std::abort();
	}

	static std::vector<fs::path> get_buckets()
	{
		const fs::path scoop = get_scoop_path();
		std::vector<fs::path> buckets;
		for (const auto& bucket : fs::directory_iterator(scoop / "buckets"))
		{
			if (bucket.is_directory() && is_directory(fs::path{bucket} / "bucket"))
				buckets.emplace_back(bucket);
		}
		return buckets;
	}

	static bool search_binaries(app_info& app, const nlohmann::json& manifest, const std::string& key_word)
	{
		nlohmann::json bin{};
		if (manifest.contains("bin"))
			bin = manifest["bin"];
		else if (manifest.contains("architecture") && manifest["architecture"].contains(system_bit)
			&& manifest["architecture"][system_bit].contains("bin"))
			bin = manifest["architecture"][system_bit]["bin"];
		else
			return false;
		switch (bin.type())
		{
		case nlohmann::json::value_t::string:
			if (util::string_find(bin.get_ref<const std::string&>(), key_word))
				app.binaries.emplace_back(std::format("{:s} > {:s}", bin.get_ref<const std::string&>(),
				                                      fs::path(bin.get_ref<const std::string&>()).stem().string()));
			break;
		case nlohmann::json::value_t::array:
			for (const auto& value : bin)
			{
				switch (value.type())
				{
				case nlohmann::json::value_t::string:
					if (util::string_find(value.get_ref<const std::string&>(), key_word))
						app.binaries.emplace_back(std::format("{:s} > {:s}",
						                                      value.get_ref<const std::string&>(),
						                                      fs::path(value.get_ref<const std::string&>()).stem().
						                                      string()));
					break;
				case nlohmann::json::value_t::array:
					switch (value.size())
					{
					case 1:
						if (util::string_find(value[0].get_ref<const std::string&>(), key_word))
							app.binaries.emplace_back(std::format("{:s} > {:s}",
							                                      value[0].get_ref<const std::string&>(),
							                                      fs::path(value[0].get_ref<const std::string&>()).
							                                      stem().string()));
						break;
					case 2:
					case 3:
						if (util::string_find(value[0].get_ref<const std::string&>(), key_word) ||
							util::string_find(value[1].get_ref<const std::string&>(), key_word))
							app.binaries.emplace_back(std::format("{:s} > {:s}",
							                                      value[0].get_ref<const std::string&>(),
							                                      value[1].get_ref<const std::string&>().empty()
								                                      ? fs::path(value[0].get_ref<const std::string&>())
								                                        .stem().string()
								                                      : value[1].get_ref<const std::string&>()));
						break;
					default: break;
					}
					break;
				case nlohmann::detail::value_t::null:
				case nlohmann::detail::value_t::object:
				case nlohmann::detail::value_t::boolean:
				case nlohmann::detail::value_t::number_integer:
				case nlohmann::detail::value_t::number_unsigned:
				case nlohmann::detail::value_t::number_float:
				case nlohmann::detail::value_t::binary:
				case nlohmann::detail::value_t::discarded:
					break;
				}
			}
			break;
		case nlohmann::detail::value_t::null:
		case nlohmann::detail::value_t::object:
		case nlohmann::detail::value_t::boolean:
		case nlohmann::detail::value_t::number_integer:
		case nlohmann::detail::value_t::number_unsigned:
		case nlohmann::detail::value_t::number_float:
		case nlohmann::detail::value_t::binary:
		case nlohmann::detail::value_t::discarded:
			break;
		}
		return !app.binaries.empty();
	}

	static bool search_shortcuts(app_info& app, const nlohmann::json& manifest, const std::string& key_word)
	{
		nlohmann::json shortcuts{};
		if (manifest.contains("shortcuts"))
			shortcuts = manifest["shortcuts"];
		else if (manifest.contains("architecture") && manifest["architecture"].contains(system_bit)
			&& manifest["architecture"][system_bit].contains("shortcuts"))
			shortcuts = manifest["architecture"][system_bit]["shortcuts"];
		else
			return false;
		if (!shortcuts.is_array())
			return false;
		for (auto shortcut : shortcuts)
		{
			if (!shortcut.is_array())
				continue;
			if (shortcut.size() < 2)
				continue;
			if (util::string_find(shortcut[0].get_ref<const std::string&>(), key_word) ||
				util::string_find(shortcut[1].get_ref<const std::string&>(), key_word))
			{
				app.shortcuts.emplace_back(std::format("{:s} > {:s}", shortcut[0].get_ref<const std::string&>(),
				                                       shortcut[1].get_ref<const std::string&>()));
			}
		}
		return !app.shortcuts.empty();
	}

	static match_result_t search_key_word(const fs::path& app_path, const std::string& key_word)
	{
		app_info app;
		std::string content = util::read_all(app_path);
		bool matched = false;
		try
		{
			nlohmann::json manifest = nlohmann::json::parse(content);
			const std::string app_name = app_path.stem().string();
			app.name = app_name;
			app.version = manifest["version"].get_ref<const std::string&>();
			if (util::string_find(app_name, key_word))
				matched = true;
			if (manifest["description"].is_string())
			{
				app.description = manifest["description"].get_ref<const std::string&>();
				if (util::string_find(app.description, key_word))
					matched = true;
			}
			if (search_binaries(app, manifest, key_word))
				matched = true;
			if (search_shortcuts(app, manifest, key_word))
				matched = true;
		}
		catch (nlohmann::json::parse_error& e)
		{
			std::cerr << app_path.generic_string() << ": " << e.what() << '\n';
			return std::nullopt;
		}
		return matched ? std::optional(app) : std::nullopt;
	}

	static search_result_t search_app(const fs::path& bucket, const std::string& key_word)
	{
		const auto path = bucket / "bucket";
		search_result_t result;
		for (const auto& it : fs::directory_iterator(path))
		{
			std::string extension = it.path().extension().string();
			std::ranges::transform(extension, extension.begin(), tolower);
			if (it.is_regular_file() && extension == ".json")
			{
				auto future = worker.submit(search_key_word, it.path(), key_word);
				result.emplace_back(std::move(future));
			}
		}

		return result;
	}
}

int wmain(int argc, wchar_t* argv[])
{
	if (argc < 2) return 0;
	std::string key_word = util::w2u(argv[1]);
	if (key_word == "--hook")
	{
		std::cout <<
			R"(function scoop { if ($args[0] -eq "search") { Scoop-Search.exe @($args | Select-Object -Skip 1) } else { scoop.ps1 @args } })";
		return 0;
	}
	SetConsoleOutputCP(CP_UTF8);
	std::vector<std::pair<std::string, std::future<scoop::search_result_t>>> list;
	std::wregex regex{LR"((.*?)/(.*))"};
	std::optional<std::wstring> special_bucket{};
	auto key_word_utf16 = util::u2w(key_word);
	if (std::wsmatch match_result; std::regex_match(key_word_utf16, match_result, regex))
	{
		special_bucket = match_result[1].str();
		key_word = util::w2u(match_result[2].str());
	}
	for (const auto& bucket : scoop::get_buckets())
	{
		if (!special_bucket)
		{
			const std::string bucket_name = bucket.stem().string();
			auto future = scoop::worker.submit(scoop::search_app, bucket, key_word);
			list.emplace_back(bucket_name, std::move(future));
			continue;
		}
		if (special_bucket.value() == bucket.stem())
		{
			const std::string bucket_name = bucket.stem().string();
			auto future = scoop::worker.submit(scoop::search_app, bucket, key_word);
			list.emplace_back(bucket_name, std::move(future));
		}
	}
	scoop::worker.wait_for_tasks();
	for (auto& [bucket, search_result] : list)
	{
		for (auto vec = search_result.get(); auto& future : vec)
		{
			if (auto app = future.get())
			{
				std::cout << hue::yellow << bucket;
				std::cout << hue::reset << "/";
				std::cout << hue::green << app.value().name
					<< hue::reset << '\n';
				std::cout << "  Version: " << hue::blue <<
					app.value().version << hue::reset << '\n';
				std::cout << "  Description: " << app.value().description << '\n';
				if (!app.value().binaries.empty())
				{
					std::cout << "  Binaries:" << '\n';
					for (const auto& bin : app.value().binaries)
					{
						std::cout << "    - " << bin << '\n';
					}
				}
				if (!app.value().shortcuts.empty())
				{
					std::cout << "  Shortcuts:" << '\n';
					for (const auto& shortcut : app.value().shortcuts)
					{
						std::cout << "    - " << shortcut << '\n';
					}
				}
			}
		}
	}
}
