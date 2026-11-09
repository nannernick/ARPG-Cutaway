#pragma once

namespace Settings
{
	// [General]
	inline bool Enabled = true;
	inline bool InteriorOnly = false;

	// [Sweep] - the "corridor" from the camera to the player's head.
	inline float HeadHeight = 120.0f;         // segment ends this far above the player's feet
	inline float CorridorRadius = 250.0f;     // shapes whose bounds come within this of the segment can hide
	inline float SweepInterval = 0.05f;       // seconds between sweeps
	inline float MinCameraDistance = 50.0f;   // closer than this: do nothing

	// [Hide]
	inline float MinHeight = 200.0f;          // shape center must be this far above the player's feet
	inline float MaxShapeRadius = 1500.0f;    // never hide shapes bigger than this (whole-room shells)
	inline float Linger = 0.4f;               // seconds a shape stays hidden after it stops qualifying

	// [Background]
	inline bool BlackFog = true;              // experiment: force interior fog colors to black

	// [Debug]
	inline bool DebugLog = true;

	void Load();
}
