#include "Settings.h"

namespace
{
	constexpr const char* kIniPath = "Data\\SKSE\\Plugins\\ARPGCutaway.ini";

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

	std::string Lower(std::string a_s)
	{
		std::transform(a_s.begin(), a_s.end(), a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a_s;
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
		Mode = static_cast<int>(GetFloat(ini, "General.Mode", static_cast<float>(Mode), 0.0f, 1.0f) + 0.5f);
		CutHeight = GetFloat(ini, "Cut.Height", CutHeight, 0.0f, 2000.0f);
		CutRadius = GetFloat(ini, "Cut.Radius", CutRadius, 0.0f, 100000.0f);
		CutAllGeometry = GetBool(ini, "Cut.AllGeometry", CutAllGeometry);
		CutSlope = GetFloat(ini, "Cut.Slope", CutSlope, 0.0f, 20.0f);
		ReanchorDistance = GetFloat(ini, "Cut.ReanchorDistance", ReanchorDistance, 0.0f, 5000.0f);
		ReanchorDelay = GetFloat(ini, "Cut.ReanchorDelay", ReanchorDelay, 0.0f, 30.0f);
		ReanchorSpeed = GetFloat(ini, "Cut.ReanchorSpeed", ReanchorSpeed, 1.0f, 100000.0f);
		AutoLift = GetBool(ini, "Cut.AutoLift", AutoLift);
		ClearDelay = GetFloat(ini, "Cut.ClearDelay", ClearDelay, 0.0f, 30.0f);
		LiftSpeed = GetFloat(ini, "Cut.LiftSpeed", LiftSpeed, 1.0f, 100000.0f);
		DropSpeed = GetFloat(ini, "Cut.DropSpeed", DropSpeed, 1.0f, 100000.0f);
		LiftMax = GetFloat(ini, "Cut.LiftMax", LiftMax, 0.0f, 100000.0f);

		CellOverrides.clear();
		for (const auto& [key, value] : ini) {
			if (key.rfind("Cells.", 0) != 0 || key.size() <= 6) {
				continue;
			}
			CellOverride entry;
			if (Lower(value) == "off") {
				entry.off = true;
			} else {
				char* end = nullptr;
				entry.height = std::strtof(value.c_str(), &end);
				if (end == value.c_str()) {
					logs::warn("settings: [Cells] {} = '{}' is not a number or 'off', ignored", key.substr(6), value);
					continue;
				}
			}
			CellOverrides[Lower(key.substr(6))] = entry;
		}

		HeadHeight = GetFloat(ini, "Sweep.HeadHeight", HeadHeight, 0.0f, 500.0f);
		CorridorRadius = GetFloat(ini, "Sweep.CorridorRadius", CorridorRadius, 0.0f, 5000.0f);
		SweepInterval = GetFloat(ini, "Sweep.Interval", SweepInterval, 0.0f, 1.0f);
		MinCameraDistance = GetFloat(ini, "Sweep.MinCameraDistance", MinCameraDistance, 0.0f, 1000.0f);

		MinHeight = GetFloat(ini, "Hide.MinHeight", MinHeight, -500.0f, 2000.0f);
		CeilingHeight = GetFloat(ini, "Hide.CeilingHeight", CeilingHeight, -500.0f, 5000.0f);
		BehindMargin = GetFloat(ini, "Hide.BehindMargin", BehindMargin, -2000.0f, 5000.0f);
		MaxShapeRadius = GetFloat(ini, "Hide.MaxShapeRadius", MaxShapeRadius, 0.0f, 100000.0f);
		Linger = GetFloat(ini, "Hide.Linger", Linger, 0.0f, 10.0f);
		Hysteresis = GetFloat(ini, "Hide.Hysteresis", Hysteresis, 0.0f, 1000.0f);

		BlackFog = GetBool(ini, "Background.BlackFog", BlackFog);
		HideSky = GetBool(ini, "Background.HideSky", HideSky);

		DebugLog = GetBool(ini, "Debug.Log", DebugLog);

		logs::info("settings: mode={} ({}) cutHeight={:.0f} cutRadius={:.0f} cutAllGeometry={}", Mode, Mode == 1 ? "cut" : "hide", CutHeight, CutRadius, CutAllGeometry);
		logs::info("settings: cutSlope={:.2f} reanchor={:.0f}u/{:.2f}s speed={:.0f} cellOverrides={}",
			CutSlope, ReanchorDistance, ReanchorDelay, ReanchorSpeed, CellOverrides.size());
		logs::info("settings: autoLift={} clearDelay={:.2f} liftSpeed={:.0f} dropSpeed={:.0f} liftMax={:.0f}",
			AutoLift, ClearDelay, LiftSpeed, DropSpeed, LiftMax);
		logs::info("settings: enabled={} interiorOnly={} head={:.0f} corridor={:.0f} interval={:.2f} minCam={:.0f}",
			Enabled, InteriorOnly, HeadHeight, CorridorRadius, SweepInterval, MinCameraDistance);
		logs::info("settings: ceilingHeight={:.0f} behindMargin={:.0f} hysteresis={:.0f}", CeilingHeight, BehindMargin, Hysteresis);
		logs::info("settings: minHeight={:.0f} maxShapeRadius={:.0f} linger={:.2f} blackFog={} hideSky={} debugLog={}",
			MinHeight, MaxShapeRadius, Linger, BlackFog, HideSky, DebugLog);
	}

	const CellOverride* FindCellOverride(const char* a_editorID)
	{
		if (!a_editorID || !a_editorID[0]) {
			return nullptr;
		}
		const auto it = CellOverrides.find(Lower(a_editorID));
		return it != CellOverrides.end() ? &it->second : nullptr;
	}
}
