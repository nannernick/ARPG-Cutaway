#pragma once

namespace Settings
{
	// [General]
	inline bool Enabled = true;
	inline bool InteriorOnly = true;
	inline int  Mode = 1;                     // 1 = horizontal cut (clip plane), 0 = hide whole meshes

	// [Cut] - Mode 1
	inline float CutHeight = 220.0f;          // cut plane height above the player's feet
	inline float CutRadius = 2500.0f;         // only meshes within this horizontal distance get cut
	inline bool  CutAllGeometry = true;       // draw every main-camera mesh with the cut projection (consistent depth)

	// [Sweep] - the "corridor" from the camera to the player's head.
	inline float HeadHeight = 120.0f;         // segment ends this far above the player's feet
	inline float CorridorRadius = 250.0f;     // shapes whose bounds come within this of the segment can hide
	inline float SweepInterval = 0.05f;       // seconds between sweeps
	inline float MinCameraDistance = 50.0f;   // closer than this: do nothing

	// [Hide]
	inline float MinHeight = 200.0f;          // shape center must be this far above the player's feet
	inline float CeilingHeight = 350.0f;      // at/above this: hidden anywhere near (roofs); below: camera side only
	inline float BehindMargin = 100.0f;       // how far behind the player a wall-height piece may be and still hide
	inline float MaxShapeRadius = 1500.0f;    // never hide shapes bigger than this (whole-room shells)
	inline float Linger = 0.4f;               // seconds a shape stays hidden after it stops qualifying
	inline float Hysteresis = 75.0f;          // once hidden, every threshold is relaxed by this much (stops flicker)

	// [Background]
	inline bool BlackFog = true;              // experiment: force interior fog colors to black
	inline bool HideSky = true;               // hide the sky dome while inside (it shows through removed roofs)

	// [Debug]
	inline bool DebugLog = true;

	void Load();
}
