#include "Project.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <magic_enum.hpp>

using namespace std;


Project::Project(string video)
	:video(video)
{
	Load();
}

Project::~Project()
{
	Save();
}


string Project::GetConfigPath()
{
	char binary[MAX_PATH];
	GetModuleFileNameA(NULL, binary, MAX_PATH);

	filesystem::path binaryPath = binary;
	filesystem::path videoPath = video;
	string configFile = binaryPath.parent_path().string() + "\\" + videoPath.filename().replace_extension().string() + ".json";

	return configFile;
}

void Project::Load()
{
	string file = GetConfigPath();
	std::ifstream i(file);
	if (i.fail())
		return;

	json j;
	i >> j;
	Load(j);
}

void Project::Load(json j)
{
	if (j["sets"].is_array() && j["sets"].size() > 0)
	{
		for (auto& s : j["sets"])
		{
			sets.push_back(TrackingSet::Unserialize(s));
		}
	}
}

void Project::Save()
{
	string file = GetConfigPath();
	ofstream o(file);
	if (o.fail())
		return;

	json j;
	Save(j);
	o << setw(4) << j << endl;
}

void Project::Save(json& j)
{
	j["sets"] = json::array();
	j["actions"] = json::array();

	for (auto& s : sets)
	{
		json& set = j["sets"][j["sets"].size()];
		s.Serialize(set);
		
		for (auto& e : s.events)
		{
			if (e.type == EventType::TET_POSITION)
			{
				json& a = j["actions"][j["actions"].size()];
				a["at"] = e.time;
				a["pos"] = (int)(e.position * 100);
			}
		}
	}
}