#include "Project.h"

#if WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <filesystem>
#include <fstream>
#include <magic_enum.hpp>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#if WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

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
#if WIN32
	GetModuleFileNameA(NULL, binary, MAX_PATH);
#else
    (void)readlink("/proc/self/exe", binary, PATH_MAX);
#endif

	filesystem::path binaryPath = binary;
	filesystem::path projectsPath = binaryPath.parent_path();
	if (projectsPath.filename() == "Debug" || projectsPath.filename() == "Release")
		projectsPath = projectsPath.parent_path();

	filesystem::path videoPath = video;
	string configFile = projectsPath.string() + "\\" + videoPath.filename().replace_extension().string() + ".json";

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
	if (j.contains("fps_max"))
		maxFPS = j["fps_max"];

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
	j["fps_max"] = maxFPS;
	j["sets"] = json::array();
	j["actions"] = json::array();

	for (auto s : sets)
	{
		json& set = j["sets"][j["sets"].size()];
		s->Serialize(set);

		vector<EventPtr> events;
		s->events->GetEvents(s->timeStart, s->timeEnd, events);
		
		for (auto& e : events)
		{
			if (e->type == EventType::TET_POSITION)
			{
				json& a = j["actions"][j["actions"].size()];
				a["at"] = e->time;
				a["pos"] = (int)(e->position * 100);
			}
		}
	}
}
