#include "JSONHandler.h"

namespace JSONHandler
{
	void Main::LoadMappings()
	{
		const auto start = std::chrono::high_resolution_clock::now();
		logger::info("Loading JSON files ({})...", (SettingsIni::bGeneral_AsynchronousStartup ? "asynchronous" : "synchronous"));

		const auto& dataFiles = MiscUtils::GetAllFiles<true>("Data\\SKSE\\RicochetFramework"sv, ".json"sv);
		
		json mergedData{};
		
		for (const auto& fileName : dataFiles) {
			try {
				std::ifstream fileStream(fileName);
				json fileData = json::parse(fileStream);
				logger::info("Parsing JSON Data In \"{}\"", fileName);
				JsonUtils::ProcessKeysWithDelimiter(fileData, '|');
				JsonUtils::MergeJsonRecursive(mergedData, fileData);
			} catch (const std::exception& e) {
				REPORT_AND_FAIL("Error while processing JSON file '{}': {}", fileName, e.what());
			}
		}

		ProcessAmmoMapping(mergedData);
		ProcessFractureMapping(mergedData);
		MiscUtils::ClearGetFormLookupCache();

		const auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> elapsed = end - start;
		logger::info("Loading JSON files ({}): DONE after {} seconds", (SettingsIni::bGeneral_AsynchronousStartup ? "asynchronous" : "synchronous"), elapsed.count());

		if (debugVerboseMode > 1) TRACE("Content of compiled JSON: {}", mergedData.dump(4));
	}

	void Main::ProcessAmmoMapping(const json& jsonData)
	{
		using namespace CoreStructure;

		logger::info("Processing Ammo Mapping...");

		int foundEntries = 0;

		if (jsonData.contains("AmmoMapping")) {
			for (const auto& item : jsonData["AmmoMapping"]) {
				AmmoMapping mapping{};

				mapping.id = item.value("ID", "");
				if (mapping.id.empty()) {
					logger::warn("AmmoMapping entry missing ID, skipping this entry");
					continue;
				}

				if (item.contains("AmmoBase")) mapping.ammoBase = ResolveForm<RE::TESAmmo>(item["AmmoBase"], false);
				if (item.contains("AmmoBroken")) mapping.ammoBroken = ResolveForm<RE::TESBoundObject>(item["AmmoBroken"], false);

				if (!mapping.ammoBase) {
					logger::warn("AmmoMapping entry '{}' missing or invalid AmmoBase, skipping this entry", mapping.id);
					continue;
				}
				SetFilterValue<float>(item, "Durability", mapping.durability);

				if (item.contains("BrokenStates") && item["BrokenStates"].is_array()) {
					for (const auto& stateJson : item["BrokenStates"]) {
						AmmoMapping::AmmoBrokenState state{};
						
						state.chances = stateJson.value("Chances", 0.0f);
						if (stateJson.contains("ProjectileSwap")) state.projectileSwap = ResolveForm<RE::BGSProjectile>(stateJson["ProjectileSwap"], false);
						ParseMappingFilterFormJson<RE::BGSProjectile, RE::BGSProjectile*>(stateJson, "ExtraProjectiles", state.extraProjectiles, false);

						mapping.brokenStates.push_back(std::move(state));
					}
				}

				ammoMappings[mapping.id] = std::move(mapping);
				foundEntries++;
			}
		}

		logger::info("Processing Ammo Mapping: DONE (found {} entries)", foundEntries);
	}

	void Main::ProcessFractureMapping(const json& jsonData)
	{
		using namespace CoreStructure;

		logger::info("Processing Fracture Mapping...");

		int foundEntries = 0;
		if (jsonData.contains("FractureMapping")) {
			for (const auto& item : jsonData["FractureMapping"]) {
				FractureMapping mapping{};
				mapping.priority = item.value("Priority", 0);
				mapping.override = item.value("Override", false);

				if (item.contains("Filters")) {
					const auto& filters = item["Filters"];
					if (filters.is_object() && !filters.is_null()) {
						ProcessFractureMappingFilters(filters, mapping.filters);
					}
				}

				if (item.contains("Modifiers")) {
					const auto& modifiers = item["Modifiers"];
					if (modifiers.is_object() && !modifiers.is_null()) {
						ProcessFractureMappingModifiers(modifiers, mapping.modifiers);
					}
				}

				arrowMappings.push_back(std::move(mapping));
				foundEntries++;
			}
		}

		logger::info("Processing Fracture Mapping: DONE (found {} entries)", foundEntries);
	}

	void Main::ProcessFractureMappingFilters(const json& filters, CoreStructure::FractureMappingFilters& mapFilters)
	{
		using namespace CoreStructure;

		ParseMappingFilterFormJson<RE::TESObjectWEAP, RE::TESObjectWEAP*>(filters, "Weapons", mapFilters.weapons, false);
		ParseMappingFilterFormJson<RE::BGSKeyword, RE::BGSKeyword*>(filters, "WeaponKeywords", mapFilters.weaponKeywords, true);
		ParseMappingFilterFormJson<RE::TESBoundObject, RE::TESBoundObject*>(filters, "Ammunitions", mapFilters.ammunitions, false);
		ParseMappingFilterFormJson<RE::BGSKeyword, RE::BGSKeyword*>(filters, "AmmunitionKeywords", mapFilters.ammunitionKeywords, true);
		ParseMappingFilterFormJson<RE::BGSProjectile, RE::BGSProjectile*>(filters, "Projectiles", mapFilters.projectiles, false);
		ParseMappingFilterFormJson<RE::BGSMaterialType, RE::BGSMaterialType*>(filters, "Materials", mapFilters.materials, false);
		ParseMappingFilterFormJson<RE::TESBoundObject, RE::TESBoundObject*>(filters, "HitObjects", mapFilters.hitObjects, false);
		SetFilterValue<float>(filters, "Percentage", mapFilters.percentage);
		SetFilterValue<float>(filters, "MinPower", mapFilters.minPower);
		SetFilterValue<float>(filters, "MaxPower", mapFilters.maxPower);
		SetFilterValue<float>(filters, "MinVelocity", mapFilters.minVelocity);
		SetFilterValue<float>(filters, "MaxVelocity", mapFilters.maxVelocity);
		SetFilterValue<bool>(filters, "ArrowSticks", mapFilters.arrowSticks);
		SetFilterValue<bool>(filters, "IsActor", mapFilters.isActor);

		if (filters.contains("Globales") && !filters["Globales"].is_null()) {
			auto entries = filters["Globales"].is_array() ? filters["Globales"] : json::array({ filters["Globales"] });
			for (const auto& globale : entries) {
				if (!globale.is_array() || globale.size() != 3) continue;
				if (!globale[0].is_string() || !globale[2].is_number()) continue;

				auto* globaleForm = Main::ResolveForm<RE::TESGlobal>(globale[0].get<std::string>(), true);
				if (!globaleForm) continue;

				std::string comparisonStr = globale[1].get<std::string>();
				ComparisonType comparison = CoreStructure::ToComparisonType(comparisonStr);

				if (comparison == ComparisonType::kInvalid) continue;
				mapFilters.globales.emplace_back(globaleForm, comparison, globale[2].get<float>());
			}
		}
	}

	void Main::ProcessFractureMappingModifiers(const json& modifiers, CoreStructure::FractureMappingModifiers& mapModifiers)
	{
		using namespace CoreStructure;

		if (modifiers.contains("AmmoID") && modifiers["AmmoID"].is_string()) {
			const std::string id = modifiers["AmmoID"].get<std::string>();
			auto it = ammoMappings.find(id);
			if (it != ammoMappings.end()) mapModifiers.ammoMapping = it->second;
			else {
				logger::warn("AmmoID '{}' not found in AmmoMappings, ignoring.", id);
				mapModifiers.ammoMapping = std::nullopt;
			}
		} else mapModifiers.ammoMapping = std::nullopt;

		SetFilterValue<bool>(modifiers, "Fracture", mapModifiers.fracture);
		SetFilterValue<bool>(modifiers, "ImpactBounce", mapModifiers.impactBounce);

		if (modifiers.contains("ImpactSound")) mapModifiers.impactSound = Main::ResolveForm<RE::BGSSoundDescriptorForm>(modifiers["ImpactSound"], false);
		ParseMappingFilterFormJson<RE::BGSSoundDescriptorForm, RE::BGSSoundDescriptorForm*>(modifiers, "ExtraImpactSounds", mapModifiers.extraImpactSounds, false);
		ParseMappingFilterFormJson<RE::BGSImpactData, RE::BGSImpactData*>(modifiers, "ExtraImpactEffects", mapModifiers.extraImpactEffects, false);
	}

	template <typename Type>
	Type* Main::ResolveForm(const std::string& str, const bool useAssociatedForm)
	{
		if (str.empty()) return nullptr;

		return (useAssociatedForm && str.find(':') == std::string::npos) ?
			MiscUtils::GetFormFromEditorID<Type>(str) : MiscUtils::GetFormFromAssoc<Type>(str);
	}

	template <typename Type, typename Output>
	void Main::ParseMappingFilterFormJson(const json& filters, const std::string& key, std::vector<Output>& out, const bool useAssociatedForm)
	{
		if (!filters.contains(key) || filters[key].is_null()) return;
		const auto& value = filters[key];
		auto entries = value.is_array() ? value : json::array({ value });

		for (const auto& entry : entries) {
			if (!entry.is_string()) continue;
			Type* formatedForm = ResolveForm<Type>(entry.get<std::string>(), useAssociatedForm);
			if (!formatedForm) continue;
			if constexpr (std::is_same_v<Output, Type*>) out.push_back(formatedForm);
			else if constexpr (std::is_same_v<Output, RE::FormID>) {
				if (auto* form = formatedForm->As<RE::TESForm>()) {
					out.push_back(form->GetFormID());
				}
			}
		}
	}

	template <typename T>
	static void Main::SetFilterValue(const json& filters, const std::string& key, std::optional<std::variant<T, RE::TESGlobal*>>& target)
	{
		if (!filters.contains(key)) return;

		const auto& value = filters[key];
		if constexpr (std::is_same_v<T, bool>) {
			if (value.is_boolean()) {
				target = value.get<bool>();
				return;
			}
		} else if constexpr (std::is_same_v<T, float>) {
			if (value.is_number()) {
				target = value.get<float>();
				return;
			}
		}

		if (value.is_string()) {
			if (auto* global = ResolveForm<RE::TESGlobal>(value, true)) {
				target = global;
			}
		}
	}
}
