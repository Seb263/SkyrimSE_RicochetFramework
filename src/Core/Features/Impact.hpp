#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Structure.h"

#include "Utils/MiscUtils.hpp"
#include "Utils/NiUtils.hpp"
#include "Utils/TimeUtils.hpp"

namespace ModCore
{
	using namespace CoreStructure;

	class Impact
	{
	public:

		static void Initialize(RE::Projectile* projectile, RE::TESObjectREFR* hitRef, const RE::hkVector4& impactPointVec)
		{
			if (!projectile) return;

			ApplyImpact(projectile, hitRef, impactPointVec);
		}

		static void Initialize(RE::Projectile* projectile, const ModUtils::CollisionData& collisionData)
		{
			if (!projectile || !collisionData.isValid) return;

			ApplyImpact(projectile, collisionData.hitRef, collisionData.hitAngle);
		}

	private:

		static void ApplyImpact(RE::Projectile* projectile, RE::TESObjectREFR* hitRef, const RE::hkVector4& impactPointVec)
		{
			if (!projectile) return;

			const auto impactPoint = MiscUtils::Normalized(MiscUtils::HkVector4ToNiPoint3(impactPointVec));
			const auto velocity = projectile->GetProjectileRuntimeData().linearVelocity;

			TimeUtils::WaitUntilRagdollReady(projectile, [=](RE::TESObjectREFR* projectileRef, bool result) {
				if (!result) return;
			
				ApplyImpactResponse(projectileRef, impactPoint, velocity);
			}, 1s);

			if (SettingsIni::fImpact_StickyRollVariance > 0.0f) {
				TimeUtils::WaitUntil3DReady(projectile, [](RE::TESObjectREFR* projectileRef, bool result) {
					if (!result || !MiscUtils::GetValidReference<RE::Projectile>(projectileRef, true) || NiUtils::IsReferenceRagdollReady(projectileRef)) return;

					if (auto* projectile3D = projectileRef->Get3D2()) {
						for (auto& child : projectile3D->AsNode()->GetChildren()) {
							if (!child) continue;
							const float randomRoll = MiscUtils::GetRandomNumber(-SettingsIni::fImpact_StickyRollVariance, SettingsIni::fImpact_StickyRollVariance);
							MiscUtils::ApplyLocalRotation(child->local.rotate, randomRoll, MiscUtils::LocalAxis::Yaw);
						}
					}
				}, 1s);
			}
		}

		static void ApplyImpactResponse(RE::TESObjectREFR* projectileRef, const RE::NiPoint3& impactPoint, const RE::NiPoint3& velocity)
		{
			if (!projectileRef) return;

			if (auto* ref3D = projectileRef->Get3D()) {
				if (auto* rigidBody = NiUtils::GetRigidBody(ref3D)) {
					if (SettingsIni::fImpact_ForcedArrowMass > 0.0f) rigidBody->motion.SetMass(SettingsIni::fImpact_ForcedArrowMass);

					RE::NiPoint3 reflectedVelocity, angularVelocity;
					const RE::NiPoint3 velocityNormalized = MiscUtils::Normalized(velocity);
					const float cosTheta = std::abs(velocityNormalized.Dot(impactPoint));

					if (cosTheta > 0.8f) {
						reflectedVelocity = MethodRebound(MiscUtils::HkVector4ToNiPoint3(rigidBody->motion.linearVelocity), velocity);
						angularVelocity = GetAngularVelocity(velocity, reflectedVelocity);

						const float randomAngularValue = SettingsIni::fImpact_AngularRandomValue * SettingsIni::fImpact_AngularImpulseMult;
						angularVelocity.x += MiscUtils::GetRandomNumber(-randomAngularValue, randomAngularValue);
						angularVelocity.y += MiscUtils::GetRandomNumber(-randomAngularValue, randomAngularValue);
						angularVelocity.z += MiscUtils::GetRandomNumber(-randomAngularValue, randomAngularValue);
					} else {
						reflectedVelocity = MethodRicochet(impactPoint, velocity);
						angularVelocity = GetAngularVelocity(velocity, reflectedVelocity);
					}

					rigidBody->SetLinearVelocity((reflectedVelocity * rigidBody->motion.GetMass()) / 25.0f);
					rigidBody->SetAngularVelocity(angularVelocity);
				}
			}
		}

		static RE::NiPoint3 GetAngularVelocity(const RE::NiPoint3& velocity, const RE::NiPoint3& reflectedVelocity)
		{
			const RE::NiPoint3 velocityNormalized = MiscUtils::Normalized(velocity);
			const RE::NiPoint3 reflectedVelocityNormalized = MiscUtils::Normalized(reflectedVelocity);

			RE::NiPoint3 rotAxis = velocityNormalized.Cross(reflectedVelocityNormalized);
			if (rotAxis != RE::NiPoint3()) rotAxis.Unitize();

			const float angle = std::acos(std::clamp(velocityNormalized.Dot(reflectedVelocityNormalized), -1.0f, 1.0f));
			const float magnitude = angle * reflectedVelocity.Length() * 0.05f;

			return rotAxis * magnitude * SettingsIni::fImpact_AngularImpulseMult;
		}

		static RE::NiPoint3 MethodRebound(const RE::NiPoint3& linearVelocity, const RE::NiPoint3& velocity)
		{
			RE::NiPoint3 reflectedVelocity = MiscUtils::Normalized(linearVelocity) * velocity.Length();
			reflectedVelocity *= 0.012f * SettingsIni::fImpact_LinearImpulseReboundMult;

			return MiscUtils::ClampNiPoint3(reflectedVelocity, SettingsIni::fImpact_LinearImpulseMin, SettingsIni::fImpact_LinearImpulseMax);
		}

		static RE::NiPoint3 MethodRicochet(const RE::NiPoint3& impactPoint, const RE::NiPoint3& velocity)
		{
			const RE::NiPoint3 velocityNormalized = MiscUtils::Normalized(velocity);
			const float dot = velocity.Dot(impactPoint);

			RE::NiPoint3 reflectedVelocity = velocity - 2.0f * dot * impactPoint;
			reflectedVelocity = MiscUtils::Normalized(reflectedVelocity) * velocity.Length();

			reflectedVelocity *= 0.012f * SettingsIni::fImpact_LinearImpulseRicochetMult;
			return MiscUtils::ClampNiPoint3(reflectedVelocity, SettingsIni::fImpact_LinearImpulseMin, SettingsIni::fImpact_LinearImpulseMax);
		}
	};
}
