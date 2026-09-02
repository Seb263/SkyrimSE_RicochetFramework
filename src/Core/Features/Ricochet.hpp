#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Structure.h"

#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"
#include "Utils/NiUtils.hpp"
#include "Utils/TimeUtils.hpp"

namespace ModCore
{
	using namespace CoreStructure;

	class Ricochet
	{
	public:

		struct ProjectileRuntime
		{
			RE::NiPoint3 velocity;
			RE::NiPoint3 linearVelocity;
			RE::ObjectRefHandle shooter;
			RE::ObjectRefHandle desiredTarget;
			RE::MagicSystem::CastingSource castingSource;
			RE::TESObjectWEAP* weaponSource;
			RE::TESAmmo* ammoSource;
			float power;
			float speedMult;
			float range;
			float livingTime;
			float weaponDamage;
			float distanceMoved;

			void CopyTo(RE::Projectile::PROJECTILE_RUNTIME_DATA& runtime) const
			{
				runtime.shooter = shooter;
				runtime.velocity = velocity;
				runtime.linearVelocity = linearVelocity;
				runtime.desiredTarget = desiredTarget;
				runtime.weaponSource = weaponSource;
				runtime.ammoSource = ammoSource;
				runtime.castingSource = castingSource;
				runtime.power = power;
				runtime.speedMult = speedMult;
				runtime.range = range;
				runtime.livingTime = livingTime;
				runtime.weaponDamage = weaponDamage;
				runtime.distanceMoved = distanceMoved;
			}

			void CopyFrom(const RE::Projectile::PROJECTILE_RUNTIME_DATA& runtime)
			{
				shooter = runtime.shooter;
				velocity = runtime.velocity;
				linearVelocity = runtime.linearVelocity;
				desiredTarget = runtime.desiredTarget;
				weaponSource = runtime.weaponSource;
				ammoSource = runtime.ammoSource;
				castingSource = runtime.castingSource;
				power = runtime.power;
				speedMult = runtime.speedMult;
				range = runtime.range;
				livingTime = runtime.livingTime;
				weaponDamage = runtime.weaponDamage;
				distanceMoved = runtime.distanceMoved;
			}
		};

		static bool Initialize(RE::Projectile* projectile, RE::TESObjectREFR* hitRef, const RE::hkVector4 impactPointVec, const RE::MATERIAL_ID materialID)
		{
			if (!projectile) return false;

			auto normalVector = MiscUtils::HkVector4ToNiPoint3(impactPointVec);

			auto* materialType = materialID != RE::MATERIAL_ID::kNone ? RE::BGSMaterialType::GetMaterialType(materialID) : nullptr;
			if (!materialType) return false;

			const bool arrowsStick = materialType->flags.any(RE::BGSMaterialType::FLAG::kArrowsStick);
			if (arrowsStick) return false;

			ProjectileRuntime projectileRuntime;
			projectileRuntime.CopyFrom(projectile->GetProjectileRuntimeData());

			const bool reflected = ReflectProjectile(projectileRuntime, normalVector);
			if (!reflected) return false;

			SKSE::GetTaskInterface()->AddTask([projectileHandle = projectile->GetHandle(), projectileRuntime, projectilePos = projectile->GetPosition()]() {
				auto* shooter = MiscUtils::ResolveHandle<RE::Actor>(projectileRuntime.shooter);
				auto* projectile = MiscUtils::ResolveHandle<RE::Projectile>(projectileHandle);
				if (!shooter || !projectile || !projectile->Is3DLoaded() || !projectileRuntime.ammoSource || !projectileRuntime.weaponSource) return;
			
				auto* ammoSource = projectileRuntime.ammoSource->As<RE::TESAmmo>();
				if (!ammoSource) return;

				auto projectileAngle = MiscUtils::DirToAngles(projectileRuntime.velocity);
				RE::Projectile::LaunchData launchData(shooter, projectilePos, projectileAngle, ammoSource, projectileRuntime.weaponSource);
				launchData.shooter = projectile;
				launchData.parentCell = projectile->GetParentCell();
				launchData.desiredTarget = nullptr;
				launchData.combatController = nullptr;
				launchData.autoAim = false;
			
				RE::ProjectileHandle newProjectileHandle;
				RE::Projectile::Launch(&newProjectileHandle, launchData);
				auto* genProjectile = newProjectileHandle ? newProjectileHandle.get().get() : nullptr;
				if (!genProjectile) return;

				TimeUtils::WaitUntil3DReady(genProjectile, [projectileRuntime](RE::TESObjectREFR* projectileRef, bool result) {
					if (!result || !projectileRef) return;
				
					auto* projectile = projectileRef->As<RE::Projectile>();
					if (!projectile) return;

					if (SettingsIni::bRicochet_TrailFade) DisableProjectileTrail(projectile);
					projectileRuntime.CopyTo(projectile->GetProjectileRuntimeData());
				});

				projectile->GetProjectileRuntimeData().flags.set(RE::Projectile::Flags::kDestroyed);
			});

			return true;
		}

		static bool Initialize(RE::Projectile* projectile, const ModUtils::CollisionData& collisionData)
		{
			if (!projectile || !collisionData.isValid) return false;

			auto normalVector = MiscUtils::HkVector4ToNiPoint3(collisionData.hitAngle);

			if (!collisionData.hitRef || (!collisionData.collidableA && !collisionData.collidableB)) return false;
			if (collisionData.hitRef->formType == RE::FormType::ActorCharacter) return false;

			auto materialID = ModUtils::GetFirstValidMaterial(std::vector<RE::hkpCollidable*>{ collisionData.collidableA, collisionData.collidableB });

			auto* materialType = materialID != RE::MATERIAL_ID::kNone ? RE::BGSMaterialType::GetMaterialType(materialID) : nullptr;
			if (!materialType) return false;

			const bool arrowsStick = materialType->flags.any(RE::BGSMaterialType::FLAG::kArrowsStick);
			if (arrowsStick) return false;

			const bool reflected = ReflectProjectile(projectile->GetProjectileRuntimeData(), normalVector);
			if (reflected) {
				if (SettingsIni::bRicochet_TrailFade) DisableProjectileTrail(projectile);
				UpdateProjectile3D(projectile, projectile->GetProjectileRuntimeData().linearVelocity);

				std::vector<RE::TESObjectREFR*> impactRefs = { collisionData.hitRef };
				if (SettingsIni::bRicochet_EffectBoundToProjectile) impactRefs.push_back(projectile);
				PlayImpactEffects(projectile, materialType, impactRefs, normalVector);
			}

			return reflected;
		}

	private:

		template <typename T>
		static bool ReflectProjectile(T& runtimeData, const RE::NiPoint3& hitNormal)
		{
			auto velocity = runtimeData.linearVelocity;

			RE::NiPoint3 normal = MiscUtils::Normalized(hitNormal);
			RE::NiPoint3 velNorm = MiscUtils::Normalized(velocity);

			const float cosTheta = std::abs(velNorm.Dot(normal));
			if (cosTheta > SettingsIni::fRicochet_MaxBounceAngle) return false;

			const float bounceFactor = std::clamp(cosTheta * SettingsIni::fRicochet_AngleInfluence, 0.0f, 1.0f);
			const float restitution = std::clamp((1.0f - bounceFactor) * SettingsIni::fRicochet_RestitutionScale,
				SettingsIni::fRicochet_RestitutionMin, SettingsIni::fRicochet_RestitutionMax);
			const float dot = velocity.Dot(normal);
		
			RE::NiPoint3 reflectedVelocity = velocity - 2.0f * dot * normal;
			reflectedVelocity *= restitution * SettingsIni::fRicochet_VelocityDamp;
			reflectedVelocity.z = std::max(reflectedVelocity.z - SettingsIni::fRicochet_ZPenalty, 0.0f);

			const float velocitySum = reflectedVelocity.Length();
			if (velocitySum < SettingsIni::fRicochet_MinVelocity) return false;		

			runtimeData.linearVelocity = reflectedVelocity;
			runtimeData.velocity = MiscUtils::Normalized(reflectedVelocity);
			runtimeData.power *= restitution / SettingsIni::fRicochet_GravityScale;
			runtimeData.speedMult *= restitution;
			runtimeData.distanceMoved = 0.0f;

			if (SettingsIni::bRicochet_DamageByRestitution) runtimeData.weaponDamage *= restitution;
			else runtimeData.weaponDamage *= SettingsIni::fRicochet_DamageMult;

			return true;
		}

		static void DisableProjectileTrail(RE::Projectile* projectile)
		{
			TimeUtils::WaitUntil3DReady(projectile, [](RE::TESObjectREFR* projectileRef, bool result) {
				if (!result || !MiscUtils::GetValidReference<RE::Projectile>(projectileRef, true) || !projectileRef->Is3DLoaded()) return;
				
				if (auto* trail = projectileRef->GetNodeByName("trailShort")) {
					if (!trail->GetAppCulled()) trail->SetAppCulled(true);
				}
			});
		}
	
		static void UpdateProjectile3D(RE::Projectile* projectile, const RE::NiPoint3 reflected)
		{
			if (auto* projectile3D = projectile->Get3D2()) {
				RE::NiPoint3 dir = reflected;
				dir.Unitize();

				projectile->data.angle.x = asin(dir.z);
				projectile->data.angle.z = atan2(dir.x, dir.y);

				MiscUtils::SetRotationMatrix(projectile3D->local.rotate, -dir.x, dir.y, dir.z);
			}
		}

		static void PlayImpactEffects(RE::Projectile* projectile, RE::BGSMaterialType* material, std::vector<RE::TESObjectREFR*>& hitRefs, const RE::NiPoint3 hitNormal)
		{
			if (!projectile || !material || hitRefs.empty()) return;

			auto& projectileRuntime = projectile->GetProjectileRuntimeData();
			auto* impactDataSet = projectileRuntime.weaponSource ? projectileRuntime.weaponSource->impactDataSet : nullptr;
			if (!impactDataSet) return;

			auto impactIt = impactDataSet->impactMap.find(material);
			if (impactIt == impactDataSet->impactMap.end()) return;

			auto* impactData = impactIt->second;
			if (!impactData) return;

			auto hitPosition = projectile->GetPosition();

			if (SettingsIni::bRicochet_PlayImpactEffect) {
				for (auto* hitRef : hitRefs) {
					if (!hitRef) continue;
					ModUtils::SpawnParticle(hitRef, impactData, hitPosition, hitNormal);
				}
			}

			if (auto* firstRef = hitRefs.front(); firstRef && SettingsIni::bRicochet_PlayImpactSound) {
				ModUtils::SpawnSound(impactData, hitPosition);
			}
		}
	};
}
