#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Main.hpp"
#include "Core/Features/Fracture.hpp"
#include "Core/Features/Impact.hpp"
#include "Core/Features/Retrieval.hpp"
#include "Core/Features/Ricochet.hpp"

#include "Utils/TimeUtils.hpp"

namespace Events
{
	using namespace ModData;
	using namespace ModCore;

	class Hooks
	{
	public:
		// Initialization of hooks and template functions
		static void InstallHooks()
		{
			auto& trampoline = SKSE::GetTrampoline();
			SKSE::AllocTrampoline(1 << 8);

			if (REL::Module::IsVR()) {
				REL::Relocation<uintptr_t> hook{ REL::RelocationID(43013, 44204) };
				_processProjectileHit = trampoline.write_call<5>(hook.address() + REL::Relocate(0x251, 0x21F), ProcessProjectileHitTemplate);
				logger::info("ProcessProjectileHit hooked at address: 0x{:X}", _processProjectileHit.address());
			} else {
				REL::Relocation<std::uintptr_t> arrowProjectileVtbl{ RE::VTABLE_ArrowProjectile[0] };
				_arrowCollission = arrowProjectileVtbl.write_vfunc(0xBE, OnArrowCollisionTemplate);
				logger::info("ArrowCollision hooked at virtual table index 0xBE. Address: 0x{:X}", _arrowCollission.address());

				REL::Relocation<std::uintptr_t> target{ REL::RelocationID(39471, 40548), REL::Relocate(0x135, 0x10D) };
				_ActivateRef = trampoline.write_call<5>(target.address(), ActivateRefTemplate);
				logger::info("ActivateRefTemplate hooked at address: 0x{:X}", _ActivateRef.address());
			}
		}

	private:

		enum class PipelineResult
		{
			Continue,
			Blocked
		};

		static bool IsProcessableArrow(RE::Projectile* projectile)
		{
			if (!projectile) return false;
			if (!projectile->GetProjectileRuntimeData().flags.any(RE::Projectile::Flags::kInited)) return false;

			auto* projectileBase = projectile->GetProjectileBase();
			return projectileBase && projectileBase->IsArrow();
		}

		static PipelineResult RunCollisionPipeline(RE::Projectile* projectile, const ModUtils::CollisionData& collisionData, bool includeRicochet = true)
		{
			if (!projectile) return PipelineResult::Blocked;

			if (IsThrottled(projectile->GetFormID())) {
				TRACE("Collision throttled (called <50ms since last time for this projectile)");
				return PipelineResult::Blocked;
			}

			auto* projectileBase = projectile->GetProjectileBase();
			const bool alreadyGenerated = generatedProjectiles.contains(projectileBase) || generatedExtraProjectiles.contains(projectileBase);

			if (!alreadyGenerated) {
				if (includeRicochet && SettingsIni::bRicochet_Status) {
					TRACE("Ricochet::Initialize");
					const bool isRicochet = Ricochet::Initialize(projectile, collisionData);
					if (isRicochet) return PipelineResult::Blocked;
				}

				if (SettingsIni::bFracture_Status) {
					TRACE("Fracture::Initialize");
					const bool isFractured = Fracture::Initialize(projectile, collisionData);
					if (isFractured) return PipelineResult::Blocked;
				}
			}

			if (SettingsIni::bImpact_Status) Impact::Initialize(projectile, collisionData);
			Retrieval::Initialize(projectile, collisionData);

			return PipelineResult::Continue;
		}

		static PipelineResult ProcessArrowCollision(RE::Projectile* projectile, RE::hkpAllCdPointCollector* allCdPointCollector)
		{
			if (!IsProcessableArrow(projectile)) return PipelineResult::Continue;
			if (Main::ShouldIgnoreHit(projectile, allCdPointCollector)) return PipelineResult::Continue;

			const auto collisionData = ModUtils::GetCollisionDataFromCollector(allCdPointCollector, projectile);
			return RunCollisionPipeline(projectile, collisionData);
		}

		static bool ActivateRefTemplate(RE::TESObjectREFR* a_ref, RE::TESObjectREFR* a_activate_trigger, uint8_t a_arg2, RE::TESBoundObject* a_object, int32_t a_count, bool a_defaultProcessingOnly)
		{
			auto* actor = a_activate_trigger ? a_activate_trigger->As<RE::Actor>() : nullptr;
			auto* projectile = a_ref ? a_ref->As<RE::Projectile>() : nullptr;

			if (actor && projectile) {
				if (Retrieval::ActivateProjectile(projectile, actor)) {
					return true;
				}
			}

			return _ActivateRef(a_ref, a_activate_trigger, a_arg2, a_object, a_count, a_defaultProcessingOnly);
		}
		static inline REL::Relocation<decltype(ActivateRefTemplate)> _ActivateRef;

		// VR path: hooked via a different engine entry point
		static void ProcessProjectileHitTemplate(RE::Projectile* aProjectile, RE::TESObjectREFR* aReference, RE::NiPoint3* aLocation,
			RE::hkVector4* impactPointVec, RE::COL_LAYER aCollisionLayer, RE::MATERIAL_ID aMaterialID, bool* aHandled)
		{
			if (aHandled && IsProcessableArrow(aProjectile) && !Main::ShouldIgnoreHit(aProjectile, aReference)) {
				auto* projectileBase = aProjectile->GetProjectileBase();
				bool isRicochet = false;

				if (!generatedProjectiles.contains(projectileBase) && !generatedExtraProjectiles.contains(projectileBase)) {
					if (SettingsIni::bRicochet_Status) {
						isRicochet = Ricochet::Initialize(aProjectile, aReference, *impactPointVec, aMaterialID);
						if (isRicochet) *aHandled = false;
					}

					if (!isRicochet && SettingsIni::bFracture_Status) {
						Fracture::Initialize(aProjectile, aReference, *impactPointVec, aMaterialID);
					}
				}

				if (!isRicochet) {
					if (SettingsIni::bImpact_Status) Impact::Initialize(aProjectile, aReference, *impactPointVec);
					Retrieval::Initialize(aProjectile, aReference);
				}
			}

			_processProjectileHit(aProjectile, aReference, aLocation, impactPointVec, aCollisionLayer, aMaterialID, aHandled);
		}
		static inline REL::Relocation<decltype(ProcessProjectileHitTemplate)> _processProjectileHit;

		static void OnArrowCollisionTemplate(RE::Projectile* projectile, RE::hkpAllCdPointCollector* allCdPointCollector)
		{
			TRACE("OnArrowCollisionTemplate");
			if (ProcessArrowCollision(projectile, allCdPointCollector) == PipelineResult::Continue) {
				_arrowCollission(projectile, allCdPointCollector);
			}
		}
		static inline REL::Relocation<decltype(OnArrowCollisionTemplate)> _arrowCollission;

		static bool IsThrottled(RE::FormID formID)
		{
			using namespace std::chrono;
			const auto now = steady_clock::now();

			std::lock_guard lock(_mutex);
			CleanupExpired(now);

			auto it = _lastCallTimes.find(formID);
			if (it != _lastCallTimes.end()) {
				const auto elapsed = duration_cast<milliseconds>(now - it->second).count();
				if (elapsed < kThrottleWindowMs) return true;
			}

			_lastCallTimes[formID] = now;
			return false;
		}

		static constexpr int64_t kThrottleWindowMs = 20;
		static constexpr int64_t kCleanupAgeMs = 500;

		static void CleanupExpired(const std::chrono::steady_clock::time_point& now)
		{
			using namespace std::chrono;
			for (auto it = _lastCallTimes.begin(); it != _lastCallTimes.end();) {
				const auto elapsed = duration_cast<milliseconds>(now - it->second).count();
				if (elapsed > kCleanupAgeMs) {
					it = _lastCallTimes.erase(it);
				} else {
					++it;
				}
			}
		}

		static inline std::mutex _mutex;
		static inline std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastCallTimes;
	};
};
