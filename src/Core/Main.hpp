#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Structure.h"
#include "Core/Features/Global.hpp"

#include "Utils/NativeUtils.hpp"
#include "Utils/NiUtils.hpp"

namespace ModCore
{
	using namespace CoreStructure;

	class Main
	{
	public:

		static void UpdateRuntimeData()
		{
			ApplyGameSettings();
			Global::Initialize();
			CollectGeneratedProjectiles();
			CalculateTrimmedLogMeanSpeed();
			ApplyBenchKeywordSetting();
			AddLowVelocityBounceFractureModifier();
		}
		
		static bool ShouldIgnoreHit(RE::Projectile* projectile, RE::hkpAllCdPointCollector* allCdPointCollector)
		{
			if (!projectile) return true;

			auto& projectileRuntime = projectile->GetProjectileRuntimeData();
			if (!projectileRuntime.ammoSource || projectileRuntime.spell) return true;

			auto hitOpt = NiUtils::GetLastHit(allCdPointCollector);
			if (!hitOpt) return false;

			if (auto [hitRef, collidableA, collidableB] = NiUtils::GetRefAndCollidables(*hitOpt, projectile); hitRef) {
				if (auto* actor = hitRef->As<RE::Actor>(); actor && actor->IsGhost()) return true;
			}
			return false;
		}

		static bool ShouldIgnoreHit(RE::Projectile* projectile, RE::TESObjectREFR* aReference)
		{
			if (!projectile) return true;

			auto& projectileRuntime = projectile->GetProjectileRuntimeData();
			if (!projectileRuntime.ammoSource || projectileRuntime.spell) return true;

			if (aReference) {
				if (auto* actor = aReference->As<RE::Actor>(); actor && actor->IsGhost()) return true;
			}

			return false;
		}

		static void AddLowVelocityBounceFractureModifier()
		{
			using namespace ModData;

			logger::info("Adding low-velocity bounce & fracture mapping...");

			arrowMappings.erase(std::remove_if(arrowMappings.begin(), arrowMappings.end(), [](const FractureMapping& mapping) {
				return mapping.priority == 999999;
			}), arrowMappings.end());

			if (SettingsIni::fImpact_StickyVelocityMin > 0.0f) {
				FractureMapping mappingBounce{};
				mappingBounce.priority = 999999;
				mappingBounce.override = true;
				mappingBounce.filters.arrowSticks = true;
				mappingBounce.filters.maxVelocity = SettingsIni::fImpact_StickyVelocityMin;
				mappingBounce.modifiers.impactBounce = true;
				arrowMappings.push_back(std::move(mappingBounce));
			}

			if (SettingsIni::fFracture_VelocityMin > 0.0f) {
				FractureMapping mappingFracture{};
				mappingFracture.priority = 999999;
				mappingFracture.override = true;
				mappingFracture.filters.maxVelocity = SettingsIni::fFracture_VelocityMin;
				mappingFracture.modifiers.fracture = false;
				arrowMappings.push_back(std::move(mappingFracture));
			}

			std::sort(arrowMappings.begin(), arrowMappings.end(), [](const FractureMapping& a, const FractureMapping& b) { return a.priority > b.priority; });

			logger::info("Adding low-velocity bounce & fracture mapping: DONE");
		}

	private:

		static void ApplyGameSettings()
		{
			logger::info("Applying Game Settings...");

			const int iArrowInventoryChance = SettingsIni::bFracture_Status ? 0 : SettingsIni::iFracture_DefaultRecoveryChance;
			NativeUtils::SetGameSetting("iArrowInventoryChance", iArrowInventoryChance);

			NativeUtils::SetGameSetting("iProjectileMaxRefCount", SettingsIni::iGlobal_MaxWorldArrows);
			NativeUtils::SetGameSetting("iMaxAttachedArrows", SettingsIni::iGlobal_MaxStickyActorArrows);

			logger::info("Applying Game Settings: DONE");
		}

		static void CollectGeneratedProjectiles()
		{
			using namespace ModData;

			logger::info("Collecting Generated Projectiles...");

			static auto setProjectileName = [](RE::BGSProjectile* projectile, RE::TESBoundObject* brokenAmmo) {
				if (!projectile || !brokenAmmo) return;

				if (auto* nameForm = brokenAmmo->As<RE::TESFullName>()) {
					if (!projectile->fullName.empty() || nameForm->fullName.empty()) return;
					projectile->SetFullName(nameForm->fullName.c_str());
				}
			};

			static auto setBrokenAmmoStats = [](RE::TESBoundObject* brokenAmmo, RE::TESAmmo* ammo) {
				if (!brokenAmmo || !ammo) return;

				if (auto* nameForm = brokenAmmo->As<RE::TESFullName>(); nameForm && nameForm->fullName.empty()) {
					const std::string ammoName = ammo->GetFullName();
					std::string brokenName = ammoName;
					if (auto brokenAmmoFormat = SettingsIni::SettingsManager().GetLanguageValue("BrokenAmmo", gameLanguage, ""); !brokenAmmoFormat.empty()) {
						brokenName = brokenAmmoFormat;
						size_t pos = brokenName.find("%s");
						if (pos != std::string::npos) brokenName.replace(pos, 2, ammoName);
					}
					nameForm->fullName = brokenName;
				}

				if (auto* valueForm = brokenAmmo->As<RE::TESValueForm>()) {
					valueForm->value = static_cast<int32_t>(ammo->value * SettingsIni::fFracture_BrokenPriceMult);
				}

				if (auto* weightForm = brokenAmmo->As<RE::TESWeightForm>()) {
					weightForm->weight = ammo->GetWeight() * SettingsIni::fFracture_BrokenWeightMult;
				}
			};

			generatedProjectiles.clear();
			generatedExtraProjectiles.clear();

			for (const auto& [id, ammoMapping] : ammoMappings) {
				setBrokenAmmoStats(ammoMapping.ammoBroken, ammoMapping.ammoBase);
				for (const auto& state : ammoMapping.brokenStates) {
					setProjectileName(state.projectileSwap, ammoMapping.ammoBroken);
					if (state.projectileSwap) generatedProjectiles.insert(state.projectileSwap);
					for (auto* extraProj : state.extraProjectiles) {
						setProjectileName(extraProj, ammoMapping.ammoBroken);
						if (extraProj) generatedExtraProjectiles.insert(extraProj);
					}
				}
			}

			logger::info("Collecting Generated Projectiles: DONE");
		}

		static void CalculateTrimmedLogMeanSpeed()
		{
			using namespace ModData;

			logger::info("Processing trimmed log-mean arrow speed...");

			std::vector<float> allSpeeds;

			for (const auto& [id, ammoMapping] : ammoMappings) {
				if (!ammoMapping.ammoBase) continue;
				if (auto* baseProjectile = ammoMapping.ammoBase->GetRuntimeData().data.projectile) {
					allSpeeds.push_back(baseProjectile->data.speed);
				}
			}

			if (allSpeeds.empty()) {
				trimmedLogMeanSpeed = 3600.0f;
				logger::info("	-> No projectiles found, using default speed {}", trimmedLogMeanSpeed);
				return;
			}

			std::sort(allSpeeds.begin(), allSpeeds.end());

			size_t n = allSpeeds.size();
			size_t k = static_cast<size_t>(0.10f * n);

			if (2 * k >= n) k = 0;

			double logSum = 0.0;
			size_t count = 0;
			for (size_t i = k; i < n - k; ++i) {
				logSum += std::log(allSpeeds[i]);
				++count;
			}

			trimmedLogMeanSpeed = static_cast<float>(std::exp(logSum / static_cast<double>(count)));

			logger::info("Processing trimmed log-mean arrow speed: DONE ({} from {} projectiles)", trimmedLogMeanSpeed, n);
		}

		static void ApplyBenchKeywordSetting()
		{
			using namespace ModData;

			static std::unordered_map<RE::BGSConstructibleObject*, RE::BGSKeyword*> benchKeywordMap;

			logger::info("Applying benchKeyword changes...");

			for (auto* constructibleObject : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSConstructibleObject>()) {
				if (!constructibleObject || !constructibleObject->createdItem) continue;

				bool isTarget = false;

				for (auto& [id, mapping] : ammoMappings) {
					if (!mapping.ammoBase || !mapping.ammoBroken) continue;
					if (constructibleObject->createdItem != mapping.ammoBase) continue;

					constructibleObject->requiredItems.ForEachContainerObject([&](RE::ContainerObject& entry) {
						if (entry.obj && entry.obj == mapping.ammoBroken) {
							isTarget = true;
							return RE::BSContainer::ForEachResult::kStop;
						}
						return RE::BSContainer::ForEachResult::kContinue;
					});

					if (isTarget) break;
				}

				if (!isTarget) continue;

				if (!benchKeywordMap.contains(constructibleObject)) {
					benchKeywordMap[constructibleObject] = constructibleObject->benchKeyword;
				}

				constructibleObject->benchKeyword = SettingsIni::bFracture_AmmoCanBeReforged ? benchKeywordMap[constructibleObject] : nullptr;
			}

			logger::info("Applying benchKeyword changes: DONE");
		}
	};
};
