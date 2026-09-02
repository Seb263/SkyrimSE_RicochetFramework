#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Structure.h"
#include "Core/Features/FractureMapping.h"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"
#include "Utils/NativeUtils.hpp"
#include "Utils/NiUtils.hpp"
#include "Utils/TimeUtils.hpp"

namespace ModCore
{
	using namespace CoreStructure;

	class Fracture
	{
	public:

		static void Initialize(RE::Projectile* projectile, RE::TESObjectREFR* hitRef, const RE::hkVector4& impactPointVec, const RE::MATERIAL_ID aMaterialID)
		{
			if (!projectile || ModUtils::IsArrowExcluded(projectile)) return;
			
			const auto projectileHandle = projectile->GetHandle();
			const auto refHandle = hitRef ? hitRef->GetHandle() : RE::ObjectRefHandle{};
			
			SKSE::GetTaskInterface()->AddTask([=]() {
				auto* projectile = MiscUtils::GetValidReference<RE::Projectile>(MiscUtils::ResolveHandle(projectileHandle), true);
				auto* hitRef = MiscUtils::GetValidReference<RE::TESObjectREFR>(MiscUtils::ResolveHandle(refHandle));
				if (!projectile) return;

				const int fractureResult = ApplyModifiers(projectile, hitRef, impactPointVec, aMaterialID);

				if (fractureResult == 0 && hitRef) {
					auto* projectileBase = projectile->GetProjectileRuntimeData().ammoSource;
					auto* actor = hitRef->As<RE::Actor>();

					if (projectileBase && actor) {
						HandleArrowRecovery(actor, projectileBase, SettingsIni::iFracture_DefaultRecoveryChance);
					}
				}
			});
		}

		static bool Initialize(RE::Projectile* projectile, const ModUtils::CollisionData& collisionData)
		{
			if (!projectile || !collisionData.isValid || ModUtils::IsArrowExcluded(projectile)) return false;
			if (!collisionData.hitRef || (!collisionData.collidableA && !collisionData.collidableB)) return false;

			auto materialID = ModUtils::GetFirstValidMaterial(std::vector<RE::hkpCollidable*>{ collisionData.collidableA, collisionData.collidableB });

			const int fractureResult = ApplyModifiers(projectile, collisionData.hitRef, collisionData.hitAngle, materialID, true);
			if (fractureResult == 0) {
				auto* projectileBase = projectile->GetProjectileRuntimeData().ammoSource;
				auto* actor = collisionData.hitRef->As<RE::Actor>();

				HandleArrowRecovery(actor, projectileBase, SettingsIni::iFracture_DefaultRecoveryChance);
			}

			return fractureResult == 2;
		}

	private:

		static void HandleArrowRecovery(RE::Actor* actor, RE::TESAmmo* ammoSource, const int recoveryChance)
		{
			if (!actor || !ammoSource) return;

			auto* object = ammoSource->As<RE::TESBoundObject>();
			if (!object) return;

			int initialCount = 0;
			if (const auto* invChanges = actor->GetInventoryChanges()) {
				initialCount = invChanges->GetCount(object, [](auto) { return true; });
			}

			TimeUtils::WaitAndCall(500ms, [initialCount, actorHandle = actor->GetHandle(), object, recoveryChance]
				(TimeUtils::CallResult result, std::chrono::nanoseconds) {
				if (result != TimeUtils::CallResult::kEndDone) return true;

				auto* actor = MiscUtils::ResolveHandle<RE::Actor>(actorHandle);
				if (!actor) return true;

				int newCount = 0;
				if (const auto* invChanges = actor->GetInventoryChanges()) {
					newCount = invChanges->GetCount(object, [](auto) { return true; });

					if (newCount > initialCount && MiscUtils::GetRandomNumber() > recoveryChance / 100.0f) {
						actor->RemoveItem(object, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
					}
				}

				return true;
			});
		}

		// -1 = error, 0 = no mapping, 1 = valid mapping but not fractured, 2 = valid mapping and fractured
		static int ApplyModifiers(RE::Projectile* projectile, RE::TESObjectREFR* hitRef,
			const RE::hkVector4& impactPointVec, const RE::MATERIAL_ID materialID, const bool deferred = false)
		{
			if (!projectile) return -1;
			
			ModRuntime_ImpactData->data.resultOverride = RE::ImpactResult::kNone;
			auto& projectileRuntime = projectile->GetProjectileRuntimeData();

			auto* materialType = materialID != RE::MATERIAL_ID::kNone ? RE::BGSMaterialType::GetMaterialType(materialID) : nullptr;

			const bool isActor = hitRef && hitRef->As<RE::Actor>();
			if (isActor && !materialType) {
				if (auto* actorRace = hitRef->As<RE::Actor>()->GetRace()) {
					materialType = actorRace->bloodImpactMaterial;
				}
			}

			if (!materialType) return 0;
			if (materialType->IsDynamicForm() && materialType->parentType) materialType = materialType->parentType;

			const auto modifiersOpt = FractureFunctions::GetArrowMappingModifiers(projectile, hitRef, materialType);
			if (!modifiersOpt) return 0;
			const auto modifiers = modifiersOpt.value();

			auto projectilePosition = projectile->GetPosition();
			auto projectileDirection = MiscUtils::HkVector4ToNiPoint3(impactPointVec);
			auto* impactData = GetImpactData(projectile, materialType);
		
			RE::Projectile* newProjectile = projectile;
			const bool isFractured = ApplyFractureModifiers(projectile, newProjectile, hitRef, materialType, projectileDirection, modifiers, deferred);
			auto& newProjectileRuntime = newProjectile->GetProjectileRuntimeData();

			// Determine if the projectile should bounce or not
			if (MiscUtils::ResolveVariantValue(modifiers.impactBounce)) {
				newProjectileRuntime.weaponSource = ModRuntime_Weap;
				ModRuntime_ImpactData->data.resultOverride = RE::ImpactResult::kBounce;	
			}

			// Spawn particles and sounds if weaponSource is null or matches ModRuntime_Weap
			if (!newProjectileRuntime.weaponSource || newProjectileRuntime.weaponSource == ModRuntime_Weap) {
				ModUtils::SpawnParticle(newProjectile, impactData, projectilePosition, projectileDirection, false);

				auto* shooter = MiscUtils::ResolveHandle<RE::Actor>(projectileRuntime.shooter);
				if (!modifiers.impactSound) ModUtils::SpawnSound(impactData, projectilePosition, shooter);
			}
			
			if (!modifiers.ammoMapping.has_value()) return 0;
			return (isFractured ? 2 : 1);
		}

		static RE::BGSImpactData* GetImpactData(RE::Projectile* projectile, RE::BGSMaterialType* material)
		{
			if (!projectile || !material) return nullptr;
			auto& projectileRuntime = projectile->GetProjectileRuntimeData();

			auto* impactDataSet = projectileRuntime.weaponSource ? projectileRuntime.weaponSource->impactDataSet : nullptr;
			if (!impactDataSet) return nullptr;

			auto impactIt = impactDataSet->impactMap.find(material);
			if (impactIt == impactDataSet->impactMap.end()) return nullptr;
		
			auto* impactData = impactIt->second;
			return impactData;
		}

		static bool ApplyFractureModifiers(RE::Projectile* projectile, RE::Projectile*& newProjectile, RE::TESObjectREFR* hitRef, RE::BGSMaterialType* materialType,
			const RE::NiPoint3& projectileDirection, const FractureMappingModifiers& modifiers, const bool deferred = false)
		{
			if (!projectile || !MiscUtils::ResolveVariantValue(modifiers.fracture)) return false;

			auto& projectileRuntime = projectile->GetProjectileRuntimeData();
			newProjectile = projectile;

			bool isFractured = false;

			auto projectilePosition = projectile->GetPosition();
		
			auto& ammoMappingOpt = modifiers.ammoMapping;
			if (modifiers.ammoMapping.has_value() && modifiers.ammoMapping.value().durability.has_value()) {
				const int materialTypeEnum = (hitRef && hitRef->As<RE::Actor>()) ? 0 : (materialType->flags.any(RE::BGSMaterialType::FLAG::kArrowsStick) ? 1 : 2);
				if (!DurabilityCheck(projectile, modifiers, materialTypeEnum, projectileDirection)) return isFractured;
			}

			if (modifiers.ammoMapping.has_value()) {
				auto& ammoMapping = *modifiers.ammoMapping;

				// Ammo swap -> set ammo source if it's a TESAmmo
				if (auto* ammo = ammoMapping.ammoBroken ? ammoMapping.ammoBroken->As<RE::TESAmmo>() : nullptr) {
					projectileRuntime.ammoSource = ammo;
				}

				// Ammo swap that is NOT of type TESAmmo -> push it using PushAmmoSwap.
				// Done BEFORE spawning extra projectiles so every debris piece below can
				// receive the same swap (see loop) instead of only the parent projectile.
				const bool isNonAmmoBrokenSwap = ammoMapping.ammoBroken && !ammoMapping.ammoBroken->As<RE::TESAmmo>();

				if (const auto* brokenState = SelectBrokenState(*modifiers.ammoMapping)) {
					// Swap projectile
					if (brokenState->projectileSwap) {
						newProjectile = ReplaceProjectile(projectile, brokenState->projectileSwap, deferred);
						isFractured = true;
					}

					// Extra projectiles
					for (auto* extraProjectileBase : brokenState->extraProjectiles) {
						if (!extraProjectileBase) continue;
						if (auto* extraProjectile = AddExtraProjectile(projectile, extraProjectileBase)) {
							SetExtraProjectileEnableState(newProjectile, extraProjectile);
							//extraProjectile->SetActivationBlocked(true);

							// Make sure this debris piece independently carries the broken-ammo
							// identity, so picking it up still gives the broken ammo even if the
							// parent projectile (e.g. the one stuck in the NPC) has since been
							// destroyed, looted, or is otherwise no longer resolvable.
							if (isNonAmmoBrokenSwap) {
								PushAmmoSwap(extraProjectile, ammoMapping.ammoBroken);
							}

							if (!SettingsIni::bFracture_PickupBrokenArrows) extraProjectile->SetDisplayName("", true);
						
							auto& extraRuntime = extraProjectile->GetProjectileRuntimeData();
							extraRuntime.weaponSource = ModRuntime_ExtraWeap;
							extraRuntime.weaponDamage = 0.0f;
							ModRuntime_ExtraImpactData->data.resultOverride = RE::ImpactResult::kBounce;
						}
					}
				}

				if (isNonAmmoBrokenSwap) {
					if (PushAmmoSwap(newProjectile, ammoMapping.ammoBroken)) {
						//newProjectile->SetActivationBlocked(true);
						if (!SettingsIni::bFracture_PickupBrokenArrows) newProjectile->SetDisplayName("", true);
					}
				}
			}

			auto& newProjectileRuntime = newProjectile->GetProjectileRuntimeData();

			// Handle impact sounds
			if (modifiers.impactSound) {
				newProjectileRuntime.weaponSource = ModRuntime_Weap;
				NativeUtils::PlaySound(modifiers.impactSound, SettingsIni::fFracture_FractureVolume, std::monostate{}, projectilePosition);
			}

			// Play all extra impact sounds
			if (SettingsIni::bFracture_PlayFractureSounds) {
				for (auto* extraSound : modifiers.extraImpactSounds) {
					if (!extraSound) continue;
					NativeUtils::PlaySound(extraSound, SettingsIni::fFracture_FractureVolume, std::monostate{}, projectilePosition);
				}
			}

			// Play all extra impact effects
			if (SettingsIni::bFracture_PlayFractureEffects) {
				for (auto* extraEffect : modifiers.extraImpactEffects) {
					if (!extraEffect) continue;
					ModUtils::SpawnParticle(newProjectile, extraEffect, projectilePosition, projectileDirection, false);
				}
			}

			return isFractured;
		}

		struct ArrowPerkVisitor : RE::PerkEntryVisitor
		{
			std::function<RE::BSContainer::ForEachResult(RE::BGSPerkEntry*)> func;
		
			ArrowPerkVisitor(std::function<RE::BSContainer::ForEachResult(RE::BGSPerkEntry*)> f) : func(f) {}
		
			virtual RE::BSContainer::ForEachResult Visit(RE::BGSPerkEntry* a_perkEntry) override
			{
				return func(a_perkEntry);
			}
		};

		static bool DurabilityCheck(RE::Projectile* projectile, const FractureMappingModifiers& modifiers, const int materialTypeEnum, const RE::NiPoint3& projectileDirection)
		{
			if (!projectile) return false;
			if (!modifiers.ammoMapping.has_value()) return false;

			auto& ammoMapping = *modifiers.ammoMapping;
			auto& projectileRuntime = projectile->GetProjectileRuntimeData();

			// Arrow durability (0.0 to 1.0)
			const float durability = MiscUtils::ResolveVariantValue(ammoMapping.durability) / 100.0f;

			// Base fracture chance with global multiplier
			float baseChance = 0.5f * SettingsIni::fFracture_GlobalMult;

			// Apply surface type multiplier
			switch (materialTypeEnum) {
				case 0: baseChance *= SettingsIni::fFracture_SurfaceMultActor; break; // Actor
				case 1: baseChance *= SettingsIni::fFracture_SurfaceMultSoft; break; // Soft surface
				default: baseChance *= SettingsIni::fFracture_SurfaceMultHard; break; // Hard surface
			}

			// Velocity factor
			float velocity = projectileRuntime.linearVelocity.Length();
			float velocityRatio = velocity / (SettingsIni::fFracture_VelocityReference > 0.0f 
				? SettingsIni::fFracture_VelocityReference : trimmedLogMeanSpeed);
			baseChance *= velocityRatio * SettingsIni::fFracture_VelocityMult;

			// Angle factor
			RE::NiPoint3 normal = MiscUtils::Normalized(projectileDirection);
			RE::NiPoint3 velNorm = MiscUtils::Normalized(projectileRuntime.linearVelocity);
			float cosTheta = std::abs(velNorm.Dot(normal));
			float angleFactorExp = std::pow(std::clamp(cosTheta, 0.0f, 1.0f), SettingsIni::fFracture_AngleExponent);
			baseChance *= 1.0f + (SettingsIni::fFracture_AngleMult - 1.0f) * angleFactorExp;

			float perkChance = 1.0f;
		
			if (auto* shooter = MiscUtils::ResolveHandle<RE::Actor>(projectileRuntime.shooter)) {

				ArrowPerkVisitor visitor([&](RE::BGSPerkEntry* perkEntry) -> RE::BSContainer::ForEachResult {
					if (!perkEntry || perkEntry->GetType() != RE::PERK_ENTRY_TYPE::kEntryPoint) return RE::BSContainer::ForEachResult::kContinue;

					RE::BGSEntryPointPerkEntry* entryPoint = (RE::BGSEntryPointPerkEntry*)perkEntry;
					if (!entryPoint || !entryPoint->IsEntryPoint(RE::BGSEntryPoint::ENTRY_POINTS::kModRecoverArrowChance)) {
						return RE::BSContainer::ForEachResult::kContinue;
					}

					if (auto it = recoverChancePerkMappings.find(entryPoint); it != recoverChancePerkMappings.end()) {
						perkChance *= it->second;
						TRACE("Perk found that affects Arrow Recovery Rate | Mult: {:.2f}", it->second);
					}

					return RE::BSContainer::ForEachResult::kContinue;
				});

				shooter->ForEachPerkEntry(RE::BGSEntryPoint::ENTRY_POINTS::kModRecoverArrowChance, visitor);
			}

			// Final clamp to min/max fracture chance
			const float finalChance = std::clamp(baseChance, SettingsIni::fFracture_ChanceMin, SettingsIni::fFracture_ChanceMax);

			// Random roll and effective fracture chance considering durability
			float randomRoll = MiscUtils::GetRandomNumber();
			float effectiveChance = (finalChance * (1.0f - durability)) / (perkChance != 0.0f ? perkChance : 1.0f);
			bool shouldFracture = (randomRoll <= effectiveChance);

			TRACE(
				"Fracture Data:\n"
				"\t[arrow durability: {}]\n"
				"\t[initial velocity: {}]\n"
				"\t[velocity ratio: {}]\n"
				"\t[exponential angle factor: {}]\n"
				"\t[perk recovery chances: {}]\n"
				"\t[base fracture chances: {}]\n"
				"\t[final fracture chances: {}]\n"
				"\t[effective fracture chances: {}]\n"
				"\t[random roll: {}]\n"
				"\t[fracture result: {}]",
				durability,
				velocity,
				velocityRatio,
				angleFactorExp,
				perkChance,
				baseChance,
				finalChance,
				effectiveChance,
				randomRoll,
				shouldFracture ? "BROKEN" : "INTACT"
			);

			return shouldFracture;
		}

		static RE::Projectile* PlaceAndCopyProjectileData(RE::Projectile* sourceProjectile, RE::BGSProjectile* newProjectileBase, const bool deferred = false)
		{
			if (!newProjectileBase || !sourceProjectile) return nullptr;

			auto& originRuntime = sourceProjectile->GetProjectileRuntimeData();
			RE::NiPoint3 angle = originRuntime.velocity;
			RE::NiPoint3 rotation{ -asinf(angle.z), 0.0f, atan2f(angle.x, angle.y) };

			auto* newProjectileRef = NativeUtils::PlaceAtMe(sourceProjectile, newProjectileBase, sourceProjectile->GetPosition(), rotation);
			if (!newProjectileRef) return nullptr;

			auto* newProjectile = newProjectileRef->As<RE::Projectile>();
			if (!newProjectile) return nullptr;

			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				newProjectile->SetOwner(player->GetActorBase());
			}

			auto& copyRuntime = newProjectile->GetProjectileRuntimeData();
			CopyProjectileRuntimeData(originRuntime, copyRuntime);
		
			if (!deferred) {
				copyRuntime.weaponSource = nullptr;
				copyRuntime.weaponDamage = 0.0f;
			}

			return newProjectile;
		}

		static RE::Projectile* ReplaceProjectile(RE::Projectile* projectile, RE::BGSProjectile* newProjectileBase, const bool deferred = false)
		{
			if (auto* newProjectile = PlaceAndCopyProjectileData(projectile, newProjectileBase, deferred)) {
				auto& originRuntime = projectile->GetProjectileRuntimeData();
				originRuntime.flags.set(RE::Projectile::Flags::kDestroyed);
				originRuntime.weaponSource = nullptr;
				if (!deferred) {
					projectile->Disable();
					projectile->SetDelete(true);
				}

				return newProjectile;
			}
			return projectile;
		}

		static RE::Projectile* AddExtraProjectile(RE::Projectile* projectile, RE::BGSProjectile* extraProjectileBase, const bool deferred = false)
		{
			if (auto* newProjectile = PlaceAndCopyProjectileData(projectile, extraProjectileBase, deferred)) {
				// Extra instructions
				return newProjectile;
			}
			return nullptr;
		}

		static const AmmoMapping::AmmoBrokenState* SelectBrokenState(const AmmoMapping& ammoMapping)
		{
			if (ammoMapping.brokenStates.empty()) return nullptr;

			float totalChances = 0.0f;
			for (const auto& state : ammoMapping.brokenStates) {
				totalChances += state.chances;
			}
			if (totalChances <= 0.0f) return nullptr;

			const float roll = static_cast<float>(rand()) / RAND_MAX * totalChances;

			float cumulative = 0.0f;
			for (const auto& state : ammoMapping.brokenStates) {
				cumulative += state.chances;
				if (roll <= cumulative) return &state;
			}

			return &ammoMapping.brokenStates.back();
		}

		static void SetExtraProjectileEnableState(RE::Projectile* parentProjectile, RE::Projectile* childProjectile)
		{
			auto& childExtraList = childProjectile->extraList;
			if (!&childExtraList) return;

			auto* enableStateParent = childExtraList.GetByType<RE::ExtraEnableStateParent>();
			if (!enableStateParent) {
				enableStateParent = RE::BSExtraData::Create<RE::ExtraEnableStateParent>();
				if (!enableStateParent) return;
				childExtraList.Add(enableStateParent);
			}

			enableStateParent->parent = parentProjectile->GetHandle();

			auto& parentExtraList = parentProjectile->extraList;
			if (!&parentExtraList) return;

			auto* enableStateChildren = parentExtraList.GetByType<RE::ExtraEnableStateChildren>();
			if (!enableStateChildren) {
				enableStateChildren = RE::BSExtraData::Create<RE::ExtraEnableStateChildren>();
				if (!enableStateChildren) return;
				parentExtraList.Add(enableStateChildren);
			}

			enableStateChildren->children.push_front(childProjectile->GetHandle());
		}

		static bool PushAmmoSwap(RE::Projectile* projectile, RE::TESBoundObject* ammoSwap)
		{
			if (!projectile || !ammoSwap) return false;

			auto& extraList = projectile->extraList;
			if (!&extraList) return false;

			auto* promotedRef = extraList.GetByType<RE::ExtraPromotedRef>();
			if (!promotedRef) {
				promotedRef = RE::BSExtraData::Create<RE::ExtraPromotedRef>();
				if (!promotedRef) return false;
				extraList.Add(promotedRef);
			}

			promotedRef->promotedRefOwners.push_back(ammoSwap);
			return true;
		}

		static void CopyProjectileRuntimeData(const RE::Projectile::PROJECTILE_RUNTIME_DATA& input, RE::Projectile::PROJECTILE_RUNTIME_DATA& output)
		{
			output.velocity = input.velocity;
			output.linearVelocity = input.linearVelocity;
			output.shooter = input.shooter;
			output.desiredTarget = input.desiredTarget;
			output.explosion = input.explosion;
			output.spell = input.spell;
			output.castingSource = input.castingSource;
			output.avEffect = input.avEffect;
			output.power = input.power;
			output.speedMult = input.speedMult;
			output.range = input.range;
			output.livingTime = input.livingTime;
			output.weaponDamage = input.weaponDamage;
			output.transparency = input.transparency;
			output.explosionTimer = input.explosionTimer;
			output.weaponSource = input.weaponSource;
			output.ammoSource = input.ammoSource;
			output.distanceMoved = input.distanceMoved;
		}
	};
}
