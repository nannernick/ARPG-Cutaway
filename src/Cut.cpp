#include "Cut.h"
#include "Settings.h"

namespace Cut
{
	namespace
	{
		std::atomic<bool>          g_active{ false };
		std::atomic<float>         g_planeZ{ 0.0f };
		std::atomic<float>         g_playerX{ 0.0f };
		std::atomic<float>         g_playerY{ 0.0f };
		std::atomic<float>         g_camX{ 0.0f };
		std::atomic<float>         g_camY{ 0.0f };
		std::atomic<float>         g_camZ{ 0.0f };
		std::atomic<std::uint32_t> g_cutPasses{ 0 };
		std::atomic<std::uint32_t> g_skipped{ 0 };

		// Pushes the (modified) camera matrices in the shadow state to the GPU constants.
		void UpdateCameraData()
		{
			using func_t = decltype(&UpdateCameraData);
			static REL::Relocation<func_t> func{ RELOCATION_ID(75472, 77258) };
			func();
		}

		bool BelongsToActor(RE::NiAVObject* a_obj)
		{
			for (auto* cur = a_obj; cur; cur = cur->parent) {
				if (auto* ref = cur->GetUserData()) {
					return ref->Is(RE::FormType::ActorCharacter);
				}
			}
			return false;
		}

		float Sign(float a_v)
		{
			return a_v > 0.0f ? 1.0f : (a_v < 0.0f ? -1.0f : 0.0f);
		}

		// Lengyel's oblique near plane, D3D (z in [0,1]) and row-vector convention:
		// replace the projection's z column so the near plane becomes a_plane
		// (view space, visible side = positive), with the far plane tilted to pass
		// through the opposite frustum corner.
		bool MakeOblique(Matrix& a_proj, const Vector4& a_plane)
		{
			Matrix inv;
			a_proj.Invert(inv);
			const Vector4 corner{ Sign(a_plane.x), Sign(a_plane.y), 1.0f, 1.0f };
			const Vector4 q = Vector4::Transform(corner, inv);
			const float   cq = a_plane.Dot(q);
			if (cq <= 1e-6f) {
				return false;  // far corner is not on the visible side (e.g. looking up): unusable
			}
			const Vector4 col4{ a_proj._14, a_proj._24, a_proj._34, a_proj._44 };
			const float   scale = col4.Dot(q) / cq;
			a_proj._13 = scale * a_plane.x;
			a_proj._23 = scale * a_plane.y;
			a_proj._33 = scale * a_plane.z;
			a_proj._43 = scale * a_plane.w;
			return true;
		}

		struct RenderPass
		{
			static void thunk(RE::BSRenderPass* a_pass, std::uint32_t a_technique, bool a_alphaTest, std::uint32_t a_renderFlags)
			{
				if (!g_active.load(std::memory_order_acquire) || !a_pass || !a_pass->geometry) {
					func(a_pass, a_technique, a_alphaTest, a_renderFlags);
					return;
				}

				auto*       geom = a_pass->geometry;
				const auto& b = geom->worldBound;
				const float planeZ = g_planeZ.load(std::memory_order_relaxed);

				// Selective mode: only meshes that cross the cut, in range, not characters.
				// AllGeometry mode skips this so every mesh writes depth with the same
				// projection; otherwise uncut meshes fail the depth test against cut ones.
				if (!Settings::CutAllGeometry) {
					const float dx = b.center.x - g_playerX.load(std::memory_order_relaxed);
					const float dy = b.center.y - g_playerY.load(std::memory_order_relaxed);
					const float range = Settings::CutRadius + b.radius;
					if (b.center.z + b.radius <= planeZ || dx * dx + dy * dy > range * range || BelongsToActor(geom)) {
						func(a_pass, a_technique, a_alphaTest, a_renderFlags);
						return;
					}
				}

				auto* shadowState = RE::BSGraphics::RendererShadowState::GetSingleton();
				if (!shadowState) {
					func(a_pass, a_technique, a_alphaTest, a_renderFlags);
					return;
				}
				auto& data = shadowState->GetRuntimeData();
				auto& cam = *reinterpret_cast<RE::BSGraphics::ViewData*>(&data.cameraData);
				const auto& eye = *reinterpret_cast<RE::NiPoint3*>(&data.posAdjust);

				// Only the main camera: skip shadow maps, reflections, cubemaps, and
				// any non-perspective projection. Also need the eye above the plane.
				const float ex = eye.x - g_camX.load(std::memory_order_relaxed);
				const float ey = eye.y - g_camY.load(std::memory_order_relaxed);
				const float ez = eye.z - g_camZ.load(std::memory_order_relaxed);
				const bool  mainCamera = ex * ex + ey * ey + ez * ez < 50.0f * 50.0f;
				const bool  perspective = std::abs(cam.projMat._34 - 1.0f) < 0.01f && std::abs(cam.projMat._44) < 0.01f;
				if (!mainCamera || !perspective || eye.z <= planeZ + 10.0f) {
					g_skipped.fetch_add(1, std::memory_order_relaxed);
					func(a_pass, a_technique, a_alphaTest, a_renderFlags);
					return;
				}

				// World plane z = planeZ, keeping everything below it, in view space.
				// Row 3 of the view matrix is world up expressed in view space.
				const Vector4 plane{ -cam.viewMat._31, -cam.viewMat._32, -cam.viewMat._33, planeZ - eye.z };

				// Every main pass in a frame has the same camera and plane, so cache the result.
				static Matrix  s_inProj{}, s_inProjU{}, s_outProj{}, s_outProjU{};
				static Vector4 s_inPlane{};
				static bool    s_cacheValid = false;
				const bool     cacheHit = s_cacheValid &&
				                      std::memcmp(&s_inProj, &cam.projMat, sizeof(Matrix)) == 0 &&
				                      std::memcmp(&s_inProjU, &cam.projMatrixUnjittered, sizeof(Matrix)) == 0 &&
				                      std::memcmp(&s_inPlane, &plane, sizeof(Vector4)) == 0;
				if (!cacheHit) {
					Matrix proj = cam.projMat;
					Matrix projU = cam.projMatrixUnjittered;
					s_cacheValid = false;
					if (!MakeOblique(proj, plane) || !MakeOblique(projU, plane)) {
						g_skipped.fetch_add(1, std::memory_order_relaxed);
						func(a_pass, a_technique, a_alphaTest, a_renderFlags);
						return;
					}
					s_inProj = cam.projMat;
					s_inProjU = cam.projMatrixUnjittered;
					s_inPlane = plane;
					s_outProj = proj;
					s_outProjU = projU;
					s_cacheValid = true;
				}
				const Matrix& proj = s_outProj;
				const Matrix& projU = s_outProjU;

				const Matrix origProj = cam.projMat;
				const Matrix origViewProj = cam.viewProjMat;
				const Matrix origProjU = cam.projMatrixUnjittered;
				const Matrix origViewProjU = cam.viewProjMatrixUnjittered;

				cam.projMat = proj;
				cam.viewProjMat = cam.viewMat * proj;
				cam.projMatrixUnjittered = projU;
				cam.viewProjMatrixUnjittered = cam.viewMat * projU;
				UpdateCameraData();

				func(a_pass, a_technique, a_alphaTest, a_renderFlags);

				cam.projMat = origProj;
				cam.viewProjMat = origViewProj;
				cam.projMatrixUnjittered = origProjU;
				cam.viewProjMatrixUnjittered = origViewProjU;
				UpdateCameraData();

				g_cutPasses.fetch_add(1, std::memory_order_relaxed);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	void Install()
	{
		const auto site = REL::RelocationID(100852, 107642).address() + REL::Relocate(0x29E, 0x28F);
		if (*reinterpret_cast<const std::uint8_t*>(site) != 0xE8) {
			logs::warn("[cut] render-pass call site {:#x} is not a call; cut mode unavailable", site);
			return;
		}
		auto& trampoline = SKSE::GetTrampoline();
		RenderPass::func = trampoline.write_call<5>(site, reinterpret_cast<std::uintptr_t>(RenderPass::thunk));
		logs::info("[cut] render-pass hook installed at {:#x}", site);
	}

	void Publish(float a_planeZ, const RE::NiPoint3& a_player, const RE::NiPoint3& a_camera)
	{
		g_planeZ.store(a_planeZ, std::memory_order_relaxed);
		g_playerX.store(a_player.x, std::memory_order_relaxed);
		g_playerY.store(a_player.y, std::memory_order_relaxed);
		g_camX.store(a_camera.x, std::memory_order_relaxed);
		g_camY.store(a_camera.y, std::memory_order_relaxed);
		g_camZ.store(a_camera.z, std::memory_order_relaxed);
		g_active.store(true, std::memory_order_release);
	}

	void Deactivate()
	{
		g_active.store(false, std::memory_order_release);
	}

	std::uint32_t TakeCutPassCount()
	{
		return g_cutPasses.exchange(0, std::memory_order_relaxed);
	}

	std::uint32_t TakeSkippedCount()
	{
		return g_skipped.exchange(0, std::memory_order_relaxed);
	}
}
