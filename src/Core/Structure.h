#pragma once

namespace CoreStructure
{
	enum class ComparisonType
	{
		kEqual,               // "=="
		kNotEqual,            // "!="
		kLessThan,            // "<"
		kGreaterThan,         // ">"
		kLessThanOrEqual,     // "<="
		kGreaterThanOrEqual,  // ">="
		kInvalid              // Invalid
	};

	static ComparisonType ToComparisonType(const std::string& comparison) {
		using namespace ModData;
		
		if (comparison == "==") return ComparisonType::kEqual;
		if (comparison == "!=" || comparison == "<>") return ComparisonType::kNotEqual;
		if (comparison == "<") return ComparisonType::kLessThan;
		if (comparison == ">") return ComparisonType::kGreaterThan;
		if (comparison == "<=") return ComparisonType::kLessThanOrEqual;
		if (comparison == ">=") return ComparisonType::kGreaterThanOrEqual;
		
		logger::error("Invalid comparison type \"{}\"", comparison);
		return ComparisonType::kInvalid;
	}

	struct AmmoMapping
	{
		struct AmmoBrokenState
		{
			float chances = 0.0f;
			RE::BGSProjectile* projectileSwap;
			std::vector<RE::BGSProjectile*> extraProjectiles;
		};

		std::string id;
		RE::TESAmmo* ammoBase;
		RE::TESBoundObject* ammoBroken;
		std::vector<AmmoBrokenState> brokenStates;
		std::optional<std::variant<float, RE::TESGlobal*>> durability;
	};

	struct FractureMappingFilters
	{
		struct GlobalFilter
		{
			RE::TESGlobal* global;
			ComparisonType comparison;
			float value;
		};

		std::vector<GlobalFilter> globales;
		std::vector<RE::TESObjectWEAP*> weapons;
		std::vector<RE::BGSKeyword*> weaponKeywords;
		std::vector<RE::TESBoundObject*> ammunitions;
		std::vector<RE::BGSKeyword*> ammunitionKeywords;
		std::vector<RE::BGSProjectile*> projectiles;
		std::vector<RE::BGSMaterialType*> materials;
		std::vector<RE::TESBoundObject*> hitObjects;
		std::optional<std::variant<float, RE::TESGlobal*>> percentage;
		std::optional<std::variant<float, RE::TESGlobal*>> minPower;
		std::optional<std::variant<float, RE::TESGlobal*>> maxPower;
		std::optional<std::variant<float, RE::TESGlobal*>> minVelocity;
		std::optional<std::variant<float, RE::TESGlobal*>> maxVelocity;
		std::optional<std::variant<bool, RE::TESGlobal*>> arrowSticks;
		std::optional<std::variant<bool, RE::TESGlobal*>> isActor;
	};

	struct FractureMappingModifiers
	{
		std::optional<std::variant<bool, RE::TESGlobal*>> fracture;
		std::optional<AmmoMapping> ammoMapping;
		RE::BGSSoundDescriptorForm* impactSound;
		std::vector<RE::BGSSoundDescriptorForm*> extraImpactSounds;
		std::vector<RE::BGSImpactData*> extraImpactEffects;
		std::optional<std::variant<bool, RE::TESGlobal*>> impactBounce;
	};

	struct FractureMapping
	{
		int priority;
		bool override;
		FractureMappingFilters filters;
		FractureMappingModifiers modifiers;
	};

	inline RE::TESAmmo* ModRuntime_Ammo;
	inline RE::TESObjectWEAP* ModRuntime_Weap;
	inline RE::BGSImpactData* ModRuntime_ImpactData;
	inline RE::BGSImpactDataSet* ModRuntime_ImpactDataSet;
	inline RE::TESObjectWEAP* ModRuntime_ExtraWeap;
	inline RE::BGSImpactData* ModRuntime_ExtraImpactData;
	inline RE::BGSImpactDataSet* ModRuntime_ExtraImpactDataSet;
	inline RE::TESObjectWEAP* ModRuntime_CloneWeap;
	inline RE::BGSImpactData* ModRuntime_CloneImpactData;
	inline RE::BGSImpactDataSet* ModRuntime_CloneImpactDataSet;
	
	inline std::vector<FractureMapping> arrowMappings;
	inline std::unordered_map<std::string, AmmoMapping> ammoMappings;
	inline std::unordered_map<RE::BGSEntryPointPerkEntry*, float> recoverChancePerkMappings;

	inline std::unordered_set<RE::BGSProjectile*> generatedProjectiles;
	inline std::unordered_set<RE::BGSProjectile*> generatedExtraProjectiles;
	inline float trimmedLogMeanSpeed;
};
