#pragma once

#include "TrackingSet.h"

#include <string>
#include <vector>

class Project
{
public:
	Project(std::string video);
	~Project();

	std::string GetConfigPath();

	void Load();
	void Load(json j);
	void Save();
	void Save(json& j);

	std::vector<std::shared_ptr<TrackingSet>> sets;
	int maxFPS = 120;
	std::string video;

protected:
	
};