#pragma once

// Horizontal cut: for meshes that reach above a plane at CutHeight over the
// player's feet, the render pass is drawn with an oblique projection whose near
// plane is that horizontal plane. Everything above it (between the camera and
// the plane) is clipped by the GPU; everything below is untouched.
namespace Cut
{
	void Install();

	// Main thread: publish this frame's cut parameters.
	void Publish(float a_planeZ, const RE::NiPoint3& a_player, const RE::NiPoint3& a_camera);
	void Deactivate();

	// Number of passes drawn with the cut since the last call.
	std::uint32_t TakeCutPassCount();
	std::uint32_t TakeSkippedCount();
}
