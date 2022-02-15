#include "Scoop-Search.h"
#include "thread_pool.hpp"
#include "console_color.h"
#include "util.h"
#include <string>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <format>
#include <Windows.h>
#include <json/json.h>


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

	thread_pool worker{8};
	std::string system_bit = util::get_system_bit();

	fs::path get_path()
	{
		fs::path scoop_path = std::getenv("SCOOP");
		if (!exists(scoop_path))
		{
			std::cerr << "error:cant find scoop path";
			std::abort();
		}
		return scoop_path;
	}

	std::vector<fs::path> get_buckets()
	{
		const fs::path scoop = get_path();
		std::vector<fs::path> buckets;
		for (const auto& bucket : fs::directory_iterator(scoop / "buckets"))
		{
			if (bucket.is_directory())
				buckets.emplace_back(bucket);
		}
		return buckets;
	}

	bool search_binaries(app_info& app, const Json::Value& manifest, const std::string& key_word)
	{
		Json::Value bin;
		if (manifest.isMember("bin"))
			bin = manifest["bin"];
		else if (manifest.isMember("architecture") && manifest["architecture"].isMember(system_bit)
			&& manifest["architecture"][system_bit].isMember("bin"))
			bin = manifest["architecture"][system_bit]["bin"];
		else
			return false;
		switch (bin.type())
			{
		case Json::stringValue:
				if (util::string_find(bin.asString(),key_word))
					app.binaries.emplace_back(std::format("{:s} > {:s}",
					                                      bin.asString(), fs::path(bin.asString()).stem().string()));
				break;
			case Json::arrayValue:
				for (const auto& value : bin)
				{
					switch (value.type())
					{
					case Json::stringValue:
						if (util::string_find(value.asString(), key_word))
							app.binaries.emplace_back(std::format("{:s} > {:s}",
							                                      value.asString(),
							                                      fs::path(value.asString()).stem().string()));
						break;
					case Json::arrayValue:
						switch (value.size())
						{
						case 1:
							if(util::string_find(value[0].asString(),key_word))
								app.binaries.emplace_back(std::format("{:s} > {:s}",
							                                      value[0].asString(),
							                                      fs::path(value[0].asString()).stem().string()));
							break;
						case 2:
						case 3:
							if (util::string_find(value[0].asString(), key_word) ||
								util::string_find(value[1].asString(), key_word))
								app.binaries.emplace_back(std::format("{:s} > {:s}",
							                                      value[0].asString(), value[1].asString()));
							break;
						default: break;
						}
						break;
					default: break;
					}
				}
				break;
			default: break;
			}
		return !app.binaries.empty();
	}

	bool search_shortcuts(app_info& app, const Json::Value& manifest, const std::string& key_word)
	{
		Json::Value shortcuts;
		if (manifest.isMember("shortcuts"))
			shortcuts = manifest["shortcuts"];
		else if (manifest.isMember("architecture") && manifest["architecture"].isMember(system_bit)
			&& manifest["architecture"][system_bit].isMember("shortcuts"))
			shortcuts = manifest["architecture"][system_bit]["shortcuts"];
		else
			return false;
		if (!shortcuts.isArray())
			return false;
		for (auto shortcut : shortcuts)
		{
			if (!shortcut.isArray())
				continue;
			if (shortcut.size() < 2)
				continue;
			if(util::string_find(shortcut[0].asString(),key_word) ||
				util::string_find(shortcut[1].asString(), key_word))
			{
				app.shortcuts.emplace_back(std::format("{:s} > {:s}", shortcut[0].asString(), shortcut[1].asString()));
			}
		}
		return !app.shortcuts.empty();
	}

	match_result_t search_key_word(const fs::path& app_path, const std::string& key_word)
	{
		Json::Reader reader;
		Json::Value manifest;
		app_info app;
		std::string content = util::read_all(app_path);
		reader.parse(content, manifest);
		const std::string app_name = app_path.stem().string();
		bool matched = false;
		app.name = app_name;
		app.version = manifest["version"].asString();
		if (util::string_find(app_name, key_word))
			matched = true;
		std::string desc = manifest["description"].asString();
		app.description = desc;
		if(util::string_find(desc,key_word))
			matched = true;
		if (search_binaries(app, manifest, key_word))
			matched = true;
		if(search_shortcuts(app, manifest, key_word))
			matched = true;
		return matched ? std::optional(app) : std::optional<app_info>(std::nullopt);
	}

	search_result_t search_app(const fs::path& bucket, const std::string& key_word)
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

int main(int argc, char* argv[])
{
	const std::string key_word = util::a2u(argv[1]);
	SetConsoleOutputCP(CP_UTF8);
	std::vector<std::pair<std::string, std::future<scoop::search_result_t>>> list;
	for (const auto& bucket : scoop::get_buckets())
	{
		const std::string bucket_name = bucket.stem().string();
		auto future = scoop::worker.submit(scoop::search_app, bucket, key_word);
		list.emplace_back(bucket_name, std::move(future));
	}

	for (auto& it : list)
	{
		auto vec = it.second.get();
		for (auto& future : vec)
		{
			auto app = future.get();
			if (app)
			{
				std::cout << console::color(console::color::yellow,false) << it.first;
				std::cout << console::color(console::color::white, false) << "/";
				std::cout << console::color(console::color::green, false) << app.value().name
				<< console::color(console::color::white, false) << std::endl;
				std::cout << "  Version: " << console::color(console::color::blue, false) <<
					app.value().version << console::color(console::color::white, false) << std::endl;
				std::cout << "  Description: " << app.value().description << std::endl;
				if (!app.value().binaries.empty())
				{
					std::cout << "  Binaries:" << std::endl;
					for (const auto& bin : app.value().binaries)
					{
						std::cout << "    - " << bin << std::endl;
					}
				}
				if(!app.value().shortcuts.empty())
				{
					std::cout << "  Shortcuts:" << std::endl;
					for (const auto& shortcut : app.value().shortcuts)
					{
						std::cout << "    - " << shortcut << std::endl;
					}
				}
			}
		}
	}
}
