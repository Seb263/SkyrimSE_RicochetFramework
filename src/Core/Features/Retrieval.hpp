#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Structure.h"
#include "Core/Features/FractureMapping.h"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"
#include "Utils/TimeUtils.hpp"

namespace ModCore
{
	using namespace CoreStructure;

	class Retrieval
	{
	public:

		static void Initialize(RE::Projectile* projectile, RE::TESObjectREFR* hitRef)
		{
			ApplyRetrieve(projectile, hitRef);
		}

		static void Initialize(RE::Projectile* projectile, const ModUtils::CollisionData& collisionData)
		{
			if (!collisionData.isValid) return;

			ApplyRetrieve(projectile, collisionData.hitRef);
		}
	
		static bool ActivateProjectile(RE::Projectile* projectile, RE::Actor* actor)
		{
			using namespace ModData;

			if (!projectile || !actor || ModUtils::IsArrowExcluded(projectile)) return false;

			RE::Projectile* parentProjectile = projectile;
			bool parentResolved = false;

			if (auto* parentExtra = projectile->extraList.GetByType<RE::ExtraEnableStateParent>(); parentExtra && parentExtra->parent) {
				if (auto* parent = MiscUtils::ResolveHandle<RE::Projectile>(parentExtra->parent)) {
					if (MiscUtils::GetValidReference<RE::Projectile>(parent, true)) {
						parentProjectile = parent;
						parentResolved = true;
					}
				}
			}

			// Prevent double-granting by fading out chained or orphaned debris projectiles and swallowing the activation.
			if (parentResolved && IsExtraProjectile(parentProjectile)) {
				parentProjectile->GetProjectileRuntimeData().flags.set(RE::Projectile::Flags::kFading);
				return true;
			}

			// Fade out chained or orphaned debris projectiles and swallow the activation to prevent broken ammo from being granted twice.
			if (!parentResolved && IsExtraProjectile(projectile)) {
				projectile->GetProjectileRuntimeData().flags.set(RE::Projectile::Flags::kFading);
				return true;
			}

			if (!parentResolved) parentProjectile = projectile;

			if (!MiscUtils::GetValidReference<RE::Projectile>(parentProjectile, true)) return false;

			auto& parentRuntime = parentProjectile->GetProjectileRuntimeData();

			auto* baseAmmo = parentRuntime.ammoSource;
			if (!baseAmmo) return false;

			const bool sourceIsBroken = projectile != parentProjectile || IsProjectileBroken(projectile);
			int intactCount = (sourceIsBroken ? 0 : 1), brokenCount = (sourceIsBroken ? 1 : 0);

			if (sourceIsBroken && !SettingsIni::bFracture_PickupBrokenArrows) return false;

			RemoveChildrenExtraArrows(parentProjectile);

			if (SettingsIni::bRetrieve_Status && (!sourceIsBroken || SettingsIni::iRetrieve_Type > 0)) {
				const float searchRadius = actor->IsInCombat() ? SettingsIni::fRetrieve_SearchRadiusCombat : SettingsIni::fRetrieve_SearchRadius;
				RE::TES::GetSingleton()->ForEachReferenceInRange(parentProjectile, searchRadius, [&](RE::TESObjectREFR* ref) {
					if (!ref) return RE::BSContainer::ForEachResult::kContinue;

					auto* projectileRef = MiscUtils::GetValidReference<RE::Projectile>(ref, true);
					if (!projectileRef || projectileRef == parentProjectile || IsExtraProjectile(projectileRef))
						return RE::BSContainer::ForEachResult::kContinue;

					const bool targetIsBroken = IsProjectileBroken(projectileRef);
					if (targetIsBroken && !SettingsIni::bFracture_PickupBrokenArrows) return RE::BSContainer::ForEachResult::kContinue;

					auto& projectileRuntime = projectileRef->GetProjectileRuntimeData();
					if (projectileRuntime.flags.none(RE::Projectile::Flags::kProcessedImpacts)) return RE::BSContainer::ForEachResult::kContinue;

					if (MatchesAmmo(projectileRef, baseAmmo, targetIsBroken, sourceIsBroken)) {
						RemoveChildrenExtraArrows(projectileRef);
						projectileRuntime.flags.set(RE::Projectile::Flags::kDestroyed);

						if (targetIsBroken) ++brokenCount;
						else ++intactCount;
					}
			
					return RE::BSContainer::ForEachResult::kContinue;
				});
			}

			if (intactCount > 0) {
				actor->AddObjectToContainer(baseAmmo, nullptr, intactCount, actor);
				if (actor->IsPlayerRef()) actor->PlayPickUpSound(baseAmmo, true, false);
			}

			if (brokenCount > 0) {
				if (auto* baseBrokenAmmo = GetAmmoFromProjectile(parentProjectile)) {
					actor->AddObjectToContainer(baseBrokenAmmo, nullptr, brokenCount, actor);
					if (actor->IsPlayerRef()) actor->PlayPickUpSound(baseBrokenAmmo, true, false);
				}
			}

			parentRuntime.flags.set(RE::Projectile::Flags::kDestroyed);

			return true;
		}

	private:

		static void ApplyRetrieve(RE::Projectile* projectile, RE::TESObjectREFR* hitRef)
		{
			using namespace ModData;

			if (!projectile || !hitRef || !SettingsIni::bFracture_Status|| ModUtils::IsArrowExcluded(projectile)) return;

			auto* actorRef = MiscUtils::GetValidReference<RE::Actor>(hitRef, true);
			auto* projectileBase = projectile->GetProjectileBase();
			if (!projectileBase || !actorRef) return;

			RE::TESBoundObject* baseAmmo = nullptr;
			bool isBroken = false;

			if (!generatedExtraProjectiles.contains(projectileBase)) {
				if (auto* ammoSwap = ModUtils::GetAmmoSwap(projectile)) {
					baseAmmo = ammoSwap;
					isBroken = true;
				} else {
					baseAmmo = projectile->GetProjectileRuntimeData().ammoSource;
					isBroken = IsProjectileBroken(projectile);
				}
			}

			if (!baseAmmo || (isBroken && !SettingsIni::bFracture_PickupBrokenArrows)) return;

			TimeUtils::WaitUntilRagdollReady(projectile, [actorHandle = actorRef->GetHandle(), baseAmmo](RE::TESObjectREFR* projectileRef, bool result) {
				if (result) return;
			
				auto* actorRef = MiscUtils::GetValidReference<RE::Actor>(MiscUtils::ResolveHandle(actorHandle), true);
				if (actorRef && baseAmmo) actorRef->AddObjectToContainer(baseAmmo, nullptr, 1, actorRef);
			}, 300ms);
		}

		static void RemoveChildrenExtraArrows(RE::Projectile* projectile)
		{
			if (!projectile) return;

			if (auto* childrenExtra = projectile->extraList.GetByType<RE::ExtraEnableStateChildren>()) {
				for (auto& childHandle : childrenExtra->children) {
					if (auto* childProjectile = MiscUtils::ResolveHandle<RE::Projectile>(childHandle)) {
						if (childProjectile == projectile) continue;
						childProjectile->GetProjectileRuntimeData().flags.set(RE::Projectile::Flags::kDestroyed);
					}
				}
			}
		}

		static RE::TESBoundObject* GetAmmoFromProjectile(RE::Projectile* projectile)
		{
			if (!projectile) return nullptr;

			if (auto* ammoSwap = ModUtils::GetAmmoSwap(projectile)) return ammoSwap;
			if (auto* ammoBroken = FractureFunctions::GetBrokenAmmoFromProjectile(projectile)) return ammoBroken;

			return projectile->GetProjectileRuntimeData().ammoSource;
		}

		static bool IsExtraProjectile(RE::Projectile* projectile)
		{
			using namespace ModData;

			auto* projectileBase = projectile->GetBaseObject() ? projectile->GetBaseObject()->As<RE::BGSProjectile>() : nullptr;
			if (!projectileBase) return false;

			return generatedExtraProjectiles.contains(projectileBase);
		}

		static bool IsProjectileBroken(RE::Projectile* projectile)
		{
			using namespace ModData;

			auto* projectileBase = projectile->GetBaseObject() ? projectile->GetBaseObject()->As<RE::BGSProjectile>() : nullptr;
			if (!projectileBase) return false;

			return generatedProjectiles.contains(projectileBase) || generatedExtraProjectiles.contains(projectileBase);
		}

		static bool MatchesAmmo(RE::Projectile* proj, RE::TESAmmo* baseAmmo, bool targetIsBroken, bool sourceIsBroken)
		{
			if (!proj || !baseAmmo || proj->GetProjectileRuntimeData().ammoSource != baseAmmo) return false;

			switch (SettingsIni::iRetrieve_Type) {
				case 0: return !targetIsBroken;
				case 1: return targetIsBroken == sourceIsBroken;
				default: return true;
			}
		}
	};
}
