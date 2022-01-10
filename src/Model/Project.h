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

	std::vector<TrackingSet> sets;

protected:
	std::string video;
};