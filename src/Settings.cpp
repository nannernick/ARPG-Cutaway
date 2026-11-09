#include "Settings.h"

namespace
{
	constexpr const char* kIniPath = "Data\\SKSE\\Plugins\\DynamicOcclusion.ini";

	std::string Trim(const std::string& a_s)
	{
		const auto first = a_s.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) {
			return {};
		}
		const auto last = a_s.find_last_not_of(" \t\r\n");
		return a_s.substr(first, last - first + 1);
	}

	std::unordered_map<std::string, std::string> Parse()
	{
		std::unordered_map<std::string, std::string> out;
		std::ifstream                                in(kIniPath);
		if (!in.is_open()) {
			logs::warn("settings: {} not found, using defaults", kIniPath);
			return out;
		}
		std::string section;
		std::string line;
		while (std::getline(in, line)) {
			line = Trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#') {
				continue;
			}
			if (line.front() == '[' && line.back() == ']') {
				section = Trim(line.substr(1, line.size() - 2));
				continue;
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			auto value = Trim(line.substr(eq + 1));
			if (const auto semi = value.find(';'); semi != std::string::npos) {
				value = Trim(value.substr(0, semi));
			}
			out[section + "." + Trim(line.substr(0, eq))] = value;
		}
		return out;
	}

	float GetFloat(const std::unordered_map<std::string, std::string>& a_ini, const char* a_key, float a_default, float a_min, float a_max)
	{
		const auto it = a_ini.find(a_key);
		if (it == a_ini.end()) {
			return a_default;
		}
		char*       end = nullptr;
		const float v = std::strtof(it->second.c_str(), &end);
		if (end == it->second.c_str()) {
			return a_default;
		}
		return std::clamp(v, a_min, a_max);
	}

	bool GetBool(const std::unordered_map<std::string, std::string>& a_ini, const char* a_key, bool a_default)
	{
		return GetFloat(a_ini, a_key, a_default ? 1.0f : 0.0f, 0.0f, 1.0f) > 0.5f;
	}
}

namespace Settings
{
	void Load()
	{
		const auto ini = Parse();

		Enabled = GetBool(ini, "General.Enabled", Enabled);
		InteriorOnly = GetBool(ini, "General.InteriorOnly", InteriorOnly);

		HeadHeight = GetFloat(ini, "Sweep.HeadHeight", HeadHeight, 0.0f, 500.0f);
		CorridorRadius = GetFloat(ini, "Sweep.CorridorRadius", CorridorRadius, 0.0f, 5000.0f);
		SweepInterval = GetFloat(ini, "Sweep.Interval", SweepInterval, 0.0f, 1.0f);
		MinCameraDistance = GetFloat(ini, "Sweep.MinCameraDistance", MinCameraDistance, 0.0f, 1000.0f);

		MinHeight = GetFloat(ini, "Hide.MinHeight", MinHeight, -500.0f, 2000.0f);
		CeilingHeight = GetFloat(ini, "Hide.CeilingHeight", CeilingHeight, -500.0f, 5000.0f);
		BehindMargin = GetFloat(ini, "Hide.BehindMargin", BehindMargin, -2000.0f, 5000.0f);
		MaxShapeRadius = GetFloat(ini, "Hide.MaxShapeRadius", MaxShapeRadius, 0.0f, 100000.0f);
		Linger = GetFloat(ini, "Hide.Linger", Linger, 0.0f, 10.0f);

		BlackFog = GetBool(ini, "Background.BlackFog", BlackFog);
		HideSky = GetBool(ini, "Background.HideSky", HideSky);

		DebugLog = GetBool(ini, "Debug.Log", DebugLog);

		logs::info("settings: enabled={} interiorOnly={} head={:.0f} corridor={:.0f} interval={:.2f} minCam={:.0f}",
			Enabled, InteriorOnly, HeadHeight, CorridorRadius, SweepInterval, MinCameraDistance);
		logs::info("settings: ceilingHeight={:.0f} behindMargin={:.0f}", CeilingHeight, BehindMargin);
		logs::info("settings: minHeight={:.0f} maxShapeRadius={:.0f} linger={:.2f} blackFog={} hideSky={} debugLog={}",
			MinHeight, MaxShapeRadius, Linger, BlackFog, HideSky, DebugLog);
	}
}
