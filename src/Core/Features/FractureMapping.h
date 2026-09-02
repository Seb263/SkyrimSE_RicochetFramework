#pragma once

#include "Core/Structure.h"

#include "Utils/MiscUtils.hpp"

namespace CoreStructure
{
	class FractureFunctions
	{
		struct MappingContext
		{
			RE::BGSProjectile* projectileBase;
			RE::BGSMaterialType* material;
			RE::TESBoundObject* hitObject;
			RE::TESObjectWEAP* weapon;
			RE::TESAmmo* ammo;
			float power;
			float velocity;
			bool isActor;
			bool arrowSticks;
		};

		public:

		static const std::optional<FractureMappingModifiers> GetArrowMappingModifiers(RE::Projectile* projectile, RE::TESObjectREFR* hitRef, RE::BGSMaterialType* materialType)
		{
			if (!projectile) return std::nullopt;

			auto& projectileRuntime = projectile->GetProjectileRuntimeData();

			MappingContext context{};
			context.projectileBase = projectile->GetProjectileBase();
			context.material = materialType;
			context.hitObject = (hitRef ? hitRef->GetBaseObject() : nullptr);
			context.weapon = projectileRuntime.weaponSource;
			context.ammo = projectileRuntime.ammoSource;
			context.power = projectileRuntime.power;
			context.velocity = projectileRuntime.linearVelocity.Length();
			context.isActor = hitRef && hitRef->As<RE::Actor>();
			context.arrowSticks = (context.material ? context.material->flags.any(RE::BGSMaterialType::FLAG::kArrowsStick) : false);

			std::optional<FractureMappingModifiers> finalModifiers;
			for (auto& mapping : arrowMappings) {
				if (ProcessMapping(context, finalModifiers, mapping)) break;
			}

			return finalModifiers;
		}

		static RE::TESBoundObject* GetBrokenAmmoFromProjectile(RE::Projectile* projectile, const bool excludesExtra = true)
		{
			if (!projectile) return nullptr;

			auto* projectileBase = projectile->GetBaseObject() ? projectile->GetBaseObject()->As<RE::BGSProjectile>() : nullptr;
			if (!projectileBase) return nullptr;

			auto* ammoSource = projectile->GetProjectileRuntimeData().ammoSource;
			if (!ammoSource) return nullptr;

			for (auto& [id, mapping] : ammoMappings) {
				if (mapping.ammoBase == ammoSource) {
					return mapping.ammoBroken;
				}
			}

			return nullptr;
		}

		private:

		static bool ProcessMapping(const MappingContext& context, auto& finalModifiers, auto& mapping)
		{
			auto& filters = mapping.filters;

			if (!filters.globales.empty() && !HasValidGlobalFilter(filters.globales)) return false;
			if (!filters.weapons.empty() && (!context.weapon || !std::ranges::contains(filters.weapons, context.weapon))) return false;
			if (!filters.weaponKeywords.empty() && (!context.weapon || !context.weapon->HasKeywordInArray(filters.weaponKeywords, false))) return false;
			if (!filters.ammunitions.empty() && (!context.ammo || !std::ranges::contains(filters.ammunitions, context.ammo))) return false;
			if (!filters.ammunitionKeywords.empty() && (!context.ammo || !context.ammo->HasKeywordInArray(filters.ammunitionKeywords, false))) return false;
			if (!filters.projectiles.empty() && (!context.projectileBase || !std::ranges::contains(filters.projectiles, context.projectileBase))) return false;
			if (!filters.materials.empty() && (!context.material || !std::ranges::contains(filters.materials, context.material))) return false;
			if (!filters.hitObjects.empty() && (!context.hitObject || !std::ranges::contains(filters.hitObjects, context.hitObject))) return false;
			if (filters.percentage.has_value() && MiscUtils::GetRandomNumber() * 100.0f >= MiscUtils::ResolveVariantValue(filters.percentage)) return false;
			if (filters.minPower.has_value() && context.power < MiscUtils::ResolveVariantValue(filters.minPower)) return false;
			if (filters.maxPower.has_value() && context.power > MiscUtils::ResolveVariantValue(filters.maxPower)) return false;
			if (filters.minVelocity.has_value() && context.velocity < MiscUtils::ResolveVariantValue(filters.minVelocity)) return false;
			if (filters.maxVelocity.has_value() && context.velocity > MiscUtils::ResolveVariantValue(filters.maxVelocity)) return false;
			if (filters.arrowSticks.has_value() && context.arrowSticks != MiscUtils::ResolveVariantValue(filters.arrowSticks)) return false;
			if (filters.isActor.has_value() && context.isActor != MiscUtils::ResolveVariantValue(filters.isActor)) return false;

			if (!finalModifiers) {
				finalModifiers = mapping.modifiers;
			} else {
				if (!finalModifiers->fracture.has_value()) finalModifiers->fracture = mapping.modifiers.fracture;
				if (!finalModifiers->ammoMapping.has_value()) finalModifiers->ammoMapping = mapping.modifiers.ammoMapping;
				if (!finalModifiers->impactSound) finalModifiers->impactSound = mapping.modifiers.impactSound;
				if (finalModifiers->extraImpactSounds.empty()) finalModifiers->extraImpactSounds = mapping.modifiers.extraImpactSounds;
				if (!finalModifiers->impactBounce.has_value()) finalModifiers->impactBounce = mapping.modifiers.impactBounce;
			}

			return !mapping.override;
		}

		static bool HasValidGlobalFilter(std::vector<CoreStructure::FractureMappingFilters::GlobalFilter> globales) {
			using ComparisonType = CoreStructure::ComparisonType;

			return std::all_of(globales.begin(), globales.end(), [](const auto& globalFilter) {
				if (!globalFilter.global) return false;

				const float globalValue = globalFilter.global->value;
				switch (globalFilter.comparison) {
					case ComparisonType::kEqual: return globalValue == globalFilter.value;
					case ComparisonType::kNotEqual: return globalValue != globalFilter.value;
					case ComparisonType::kLessThan: return globalValue < globalFilter.value;
					case ComparisonType::kGreaterThan: return globalValue > globalFilter.value;
					case ComparisonType::kLessThanOrEqual: return globalValue <= globalFilter.value;
					case ComparisonType::kGreaterThanOrEqual: return globalValue >= globalFilter.value;
				}
				return false;
			});
		}
	};
};
