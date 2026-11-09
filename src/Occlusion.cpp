#include "Occlusion.h"
#include "Settings.h"

// Dynamic occlusion, v0.
//
// Every few frames we build a "corridor": the segment from the camera to the
// player's head, padded by CorridorRadius. Every loaded reference whose 3D
// reaches into that corridor is walked down to its individual meshes
// (BSGeometry). A mesh is hidden (app-culled) when:
//   - its bound center is at least MinHeight above the player's feet,
//   - its bound radius is <= MaxShapeRadius (so whole-room shells survive),
//   - its bound comes within CorridorRadius of the segment.
// Hidden meshes come back Linger seconds after they stop qualifying, and
// everything is restored on cell change and on save load.

namespace Occlusion
{
	namespace
	{
		constexpr auto          kContinue = RE::BSContainer::ForEachResult::kContinue;
		constexpr std::uint32_t kMaxShapeLogsPerCell = 400;
		constexpr std::uint32_t kMaxShapesPerRef = 512;

		struct HiddenEntry
		{
			RE::NiPointer<RE::NiAVObject> keepalive;
			bool                          wasCulled{ false };
			std::uint32_t                 lastSweep{ 0 };
			float                         unseenTime{ 0.0f };
		};

		struct FogBackup
		{
			RE::INTERIOR_DATA* data{ nullptr };
			RE::Color          nearColor{};
			RE::Color          farColor{};
		};

		std::unordered_map<RE::NiAVObject*, HiddenEntry> g_hidden;
		std::unordered_set<RE::FormID>                  g_loggedRefs;
		std::vector<FogBackup>                          g_fogBackups;
		RE::TESObjectCELL*                              g_lastCell{ nullptr };
		std::uint32_t                                   g_sweepId{ 0 };
		float                                           g_sweepTimer{ 0.0f };
		std::uint32_t                                   g_loggedShapes{ 0 };
		bool                                            g_skyHidden{ false };

		float         g_perfTimer{ 0.0f };
		std::uint32_t g_perfSweeps{ 0 };
		std::uint64_t g_perfRefs{ 0 };
		double        g_perfMs{ 0.0 };

		float DistSqToSegment(const RE::NiPoint3& a_p, const RE::NiPoint3& a_a, const RE::NiPoint3& a_b)
		{
			const float abx = a_b.x - a_a.x, aby = a_b.y - a_a.y, abz = a_b.z - a_a.z;
			const float apx = a_p.x - a_a.x, apy = a_p.y - a_a.y, apz = a_p.z - a_a.z;
			const float len2 = abx * abx + aby * aby + abz * abz;
			float       t = len2 > 1e-6f ? (apx * abx + apy * aby + apz * abz) / len2 : 0.0f;
			t = std::clamp(t, 0.0f, 1.0f);
			const float dx = a_p.x - (a_a.x + abx * t);
			const float dy = a_p.y - (a_a.y + aby * t);
			const float dz = a_p.z - (a_a.z + abz * t);
			return dx * dx + dy * dy + dz * dz;
		}

		std::string ColorHex(const RE::Color& a_c)
		{
			return std::format("#{:02X}{:02X}{:02X}", a_c.red, a_c.green, a_c.blue);
		}

		const char* SafeStr(const char* a_s)
		{
			return a_s ? a_s : "";
		}

		const char* ModelPath(RE::TESObjectREFR* a_ref)
		{
			auto*       base = a_ref->GetBaseObject();
			auto*       model = base ? base->As<RE::TESModel>() : nullptr;
			const char* path = model ? model->GetModel() : nullptr;
			return (path && path[0]) ? path : "<none>";
		}

		template <class F>
		void ForEachShape(RE::NiAVObject* a_obj, F& a_fn, std::uint32_t& a_budget)
		{
			if (!a_obj || a_budget == 0) {
				return;
			}
			if (auto* geom = a_obj->AsGeometry()) {
				--a_budget;
				a_fn(static_cast<RE::NiAVObject*>(geom));
				return;
			}
			if (auto* node = a_obj->AsNode()) {
				for (auto& child : node->GetChildren()) {
					if (child) {
						ForEachShape(child.get(), a_fn, a_budget);
					}
				}
			}
		}

		void RestoreEntry(const HiddenEntry& a_entry)
		{
			if (a_entry.wasCulled) {
				return;  // it was already hidden by something else; leave it alone
			}
			if (auto* live = a_entry.keepalive.get()) {
				live->SetAppCulled(false);
			}
		}

		void RestoreShapes()
		{
			for (auto& [obj, entry] : g_hidden) {
				RestoreEntry(entry);
			}
			g_hidden.clear();
		}

		void RestoreFog()
		{
			for (auto& backup : g_fogBackups) {
				if (backup.data) {
					backup.data->fogColorNear = backup.nearColor;
					backup.data->fogColorFar = backup.farColor;
				}
			}
			g_fogBackups.clear();
		}

		void BackupAndBlacken(RE::INTERIOR_DATA* a_data)
		{
			if (!a_data) {
				return;
			}
			for (const auto& backup : g_fogBackups) {
				if (backup.data == a_data) {
					return;
				}
			}
			g_fogBackups.push_back(FogBackup{ a_data, a_data->fogColorNear, a_data->fogColorFar });
			a_data->fogColorNear.red = 0;
			a_data->fogColorNear.green = 0;
			a_data->fogColorNear.blue = 0;
			a_data->fogColorFar.red = 0;
			a_data->fogColorFar.green = 0;
			a_data->fogColorFar.blue = 0;
		}

		// The sky dome still renders behind interiors. With the roof gone it is what
		// shows through (night sky = dark blue), so hide it while inside.
		void SetSkyHidden(bool a_hide)
		{
			auto* sky = RE::Sky::GetSingleton();
			auto* root = sky ? sky->root.get() : nullptr;
			if (!root) {
				return;
			}
			if (a_hide) {
				if (!g_skyHidden && Settings::DebugLog) {
					logs::info("[sky] hidden");
				}
				root->SetAppCulled(true);  // every frame, in case the engine flips it back
				g_skyHidden = true;
			} else if (g_skyHidden) {
				root->SetAppCulled(false);
				g_skyHidden = false;
				if (Settings::DebugLog) {
					logs::info("[sky] restored");
				}
			}
		}

		void OnCellChanged(RE::TESObjectCELL* a_cell)
		{
			RestoreShapes();
			RestoreFog();
			g_loggedRefs.clear();
			g_loggedShapes = 0;

			if (!a_cell) {
				return;
			}

			const bool interior = a_cell->IsInteriorCell();
			auto*      lighting = interior ? a_cell->GetLighting() : nullptr;
			auto*      tmpl = interior ? a_cell->GetRuntimeData().lightingTemplate : nullptr;

			if (Settings::DebugLog) {
				logs::info("[cell] '{}' ({:08X}) interior={}", SafeStr(a_cell->GetFormEditorID()), a_cell->GetFormID(), interior);
				if (lighting) {
					logs::info("[cell]   own fog near={} far={}", ColorHex(lighting->fogColorNear), ColorHex(lighting->fogColorFar));
				}
				if (tmpl) {
					logs::info("[cell]   template {:08X} fog near={} far={}", tmpl->GetFormID(), ColorHex(tmpl->data.fogColorNear), ColorHex(tmpl->data.fogColorFar));
				}
			}

			if (interior && Settings::BlackFog) {
				BackupAndBlacken(lighting);
				if (tmpl) {
					BackupAndBlacken(&tmpl->data);
				}
				logs::info("[fog] interior fog set to black ({} block(s))", g_fogBackups.size());
			}
		}

		void Sweep(RE::PlayerCharacter* a_player, float a_dt)
		{
			const auto start = std::chrono::steady_clock::now();

			auto* camera = RE::PlayerCamera::GetSingleton();
			auto* camNode = camera ? camera->cameraRoot.get() : nullptr;
			if (!camNode) {
				return;
			}

			const RE::NiPoint3 camPos = camNode->world.translate;
			const RE::NiPoint3 feet = a_player->GetPosition();
			const RE::NiPoint3 head{ feet.x, feet.y, feet.z + Settings::HeadHeight };
			const float        sx = head.x - camPos.x, sy = head.y - camPos.y, sz = head.z - camPos.z;
			const float        segLen = std::sqrt(sx * sx + sy * sy + sz * sz);

			++g_sweepId;
			std::uint64_t refsVisited = 0;

			auto* tes = RE::TES::GetSingleton();
			if (tes && segLen >= Settings::MinCameraDistance) {
				const float corridor = Settings::CorridorRadius;
				const float minZ = feet.z + Settings::MinHeight;

				tes->ForEachReferenceInRange(a_player, segLen + corridor, [&](RE::TESObjectREFR* a_ref) {
					++refsVisited;
					if (!a_ref || a_ref == a_player || a_ref->IsDisabled() || a_ref->Is(RE::FormType::ActorCharacter)) {
						return kContinue;
					}
					auto* root = a_ref->Get3D();
					if (!root) {
						return kContinue;
					}
					const auto& rb = root->worldBound;
					if (rb.center.z + rb.radius < minZ) {
						return kContinue;  // entirely below the cutoff height
					}
					const float rootReach = corridor + rb.radius;
					if (DistSqToSegment(rb.center, camPos, head) > rootReach * rootReach) {
						return kContinue;  // nowhere near the corridor
					}

					const bool logRef = Settings::DebugLog && g_loggedShapes < kMaxShapeLogsPerCell &&
					                    g_loggedRefs.insert(a_ref->GetFormID()).second;
					if (logRef) {
						logs::info("[ref] {:08X} model={} rootR={:.0f} rootZ={:+.0f}",
							a_ref->GetFormID(), ModelPath(a_ref), rb.radius, rb.center.z - feet.z);
					}

					auto onShape = [&](RE::NiAVObject* a_shape) {
						const auto& b = a_shape->worldBound;
						const float reach = corridor + b.radius;
						const bool  high = b.center.z >= minZ;
						const bool  notTooBig = b.radius <= Settings::MaxShapeRadius;
						const bool  inCorridor = DistSqToSegment(b.center, camPos, head) <= reach * reach;
						const bool  hide = high && notTooBig && inCorridor;

						if (logRef && g_loggedShapes < kMaxShapeLogsPerCell) {
							++g_loggedShapes;
							logs::info("[shape]   '{}' z={:+.0f} r={:.0f} high={} notTooBig={} inCorridor={} -> hide={}",
								SafeStr(a_shape->name.c_str()), b.center.z - feet.z, b.radius, high, notTooBig, inCorridor, hide);
						}
						if (!hide) {
							return;
						}

						auto [it, inserted] = g_hidden.try_emplace(a_shape);
						if (inserted) {
							it->second.keepalive = RE::NiPointer<RE::NiAVObject>(a_shape);
							it->second.wasCulled = a_shape->GetAppCulled();
							if (!it->second.wasCulled) {
								a_shape->SetAppCulled(true);
							}
						}
						it->second.lastSweep = g_sweepId;
						it->second.unseenTime = 0.0f;
					};

					std::uint32_t budget = kMaxShapesPerRef;
					ForEachShape(root, onShape, budget);
					return kContinue;
				});
			}

			// Bring back pieces that stopped qualifying.
			for (auto it = g_hidden.begin(); it != g_hidden.end();) {
				auto& entry = it->second;
				if (entry.lastSweep != g_sweepId) {
					entry.unseenTime += a_dt;
					if (entry.unseenTime >= Settings::Linger) {
						RestoreEntry(entry);
						it = g_hidden.erase(it);
						continue;
					}
				}
				++it;
			}

			++g_perfSweeps;
			g_perfRefs += refsVisited;
			g_perfMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
		}

		void Tick(RE::Actor* a_actor, float a_delta)
		{
			if (!Settings::Enabled) {
				return;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || a_actor != player) {
				return;
			}
			if (!player->Is3DLoaded()) {
				RestoreAll();
				return;
			}

			auto* cell = player->GetParentCell();
			if (cell != g_lastCell) {
				OnCellChanged(cell);
				g_lastCell = cell;
			}
			if (!cell) {
				return;
			}
			SetSkyHidden(Settings::HideSky && cell->IsInteriorCell());
			if (Settings::InteriorOnly && !cell->IsInteriorCell()) {
				RestoreShapes();
				return;
			}

			g_perfTimer += a_delta;
			if (g_perfTimer >= 2.0f) {
				if (Settings::DebugLog && g_perfSweeps > 0) {
					logs::info("[perf] sweeps/s={:.1f} refs/sweep={} hidden={} ms/sweep={:.3f}",
						g_perfSweeps / g_perfTimer, g_perfRefs / g_perfSweeps, g_hidden.size(), g_perfMs / g_perfSweeps);
				}
				g_perfTimer = 0.0f;
				g_perfSweeps = 0;
				g_perfRefs = 0;
				g_perfMs = 0.0;
			}

			g_sweepTimer += a_delta;
			if (g_sweepTimer < Settings::SweepInterval) {
				return;
			}
			const float dt = g_sweepTimer;
			g_sweepTimer = 0.0f;
			Sweep(player, dt);
		}

		struct PlayerUpdate
		{
			static void thunk(RE::Actor* a_this, float a_delta)
			{
				func(a_this, a_delta);
				Tick(a_this, a_delta);
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
		PlayerUpdate::func = vtbl.write_vfunc(0xAD, PlayerUpdate::thunk);
		logs::info("installed PlayerCharacter::Update hook (vtbl[0xAD])");
	}

	void RestoreAll()
	{
		RestoreShapes();
		RestoreFog();
		SetSkyHidden(false);
		g_lastCell = nullptr;
	}
}
