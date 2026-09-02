#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Structure.h"

namespace ModCore
{
	using namespace CoreStructure;

	class Global
	{
	public:

		static void Initialize()
		{
			for (auto* proj : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSProjectile>()) {
				if (!proj || !proj->IsArrow()) continue;

				BackupOriginalProjectileValues(proj);
				ApplyProjectileMultipliers(proj);
			}
		}

	private:

		static inline std::unordered_map<RE::BGSProjectile*, std::tuple<float, float, float, float>> projectileMap;
		static void BackupOriginalProjectileValues(RE::BGSProjectile* proj)
		{
			if (!proj) return;

			if (!projectileMap.contains(proj)) {
				projectileMap[proj] = {
					proj->data.speed,
					proj->data.gravity,
					proj->data.range,
					proj->data.force
				};
			}
		}

		static void ApplyProjectileMultipliers(RE::BGSProjectile* proj)
		{
			if (!proj) return;
			if (!projectileMap.contains(proj)) return;

			auto [origSpeed, origGravity, origRange, origForce] = projectileMap[proj];

			proj->data.speed = origSpeed * SettingsIni::fGlobal_ProjectileSpeedMult;
			proj->data.gravity = origGravity * SettingsIni::fGlobal_ProjectileGravityMult;
			proj->data.range = origRange * SettingsIni::fGlobal_ProjectileRangeMult;
			proj->data.force = origForce * SettingsIni::fGlobal_ProjectileForceMult;
		}
	};
}
