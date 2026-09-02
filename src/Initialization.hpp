#pragma once

#include "DataHandler.hpp"
#include "Events.h"
#include "Hooks.hpp"
#include "JSONHandler.h"
#include "SettingsIni.hpp"

#include "Core/Structure.h"
#include "Core/Main.hpp"

#include "Utils/TimeUtils.hpp"

namespace ModData
{
	class DataHandler
	{
	public:
		bool preLoaded = false;
		bool postLoaded = false;
		bool postLoadedAlternate = false;

		static DataHandler* GetSingleton()
		{
			static DataHandler singleton;
			return &singleton;
		}

		std::future<void> loadFuture;

		void WaitUntilReady()
		{
			if (SettingsIni::bGeneral_AsynchronousStartup && loadFuture.valid()) {
				loadFuture.get();
			}
		}

		void PreLoadData()
		{
			if (preLoaded) return;
			preLoaded = true;

			TESdataHandler = RE::TESDataHandler::GetSingleton();

			gameLanguage = NativeUtils::GetGameINISetting<std::string>("sLanguage:General").value_or("english");
			std::transform(gameLanguage.begin(), gameLanguage.end(), gameLanguage.begin(), ::tolower);

			ExtractGameAssets();
			LoadPluginsForms();
			ScanArrowRecoveryPerks();
			Events::ModEventSink::LoadEvents();

			auto loadAndInitialize = []() {
				SKSE::GetTaskInterface()->AddTask([]() {
					JSONHandler::Main::LoadMappings();
				});
			};

			if (SettingsIni::bGeneral_AsynchronousStartup) loadFuture = std::async(std::launch::async, loadAndInitialize);
			else loadAndInitialize();
		}

		void PostLoadData()
		{
			if (postLoaded) return;
			postLoaded = true;

			WaitUntilReady();

			Events::Hooks::InstallHooks();

			SKSE::GetTaskInterface()->AddTask([]() {
				ModCore::Main::UpdateRuntimeData();
			});

			ModData::DataHandler::InitializeProcessRuntimeForms();
		}

		void PostLoadDataAlternate()
		{
			if (postLoadedAlternate) return;
			postLoadedAlternate = true;

			TimeUtils::DoWhile(100ms, [](TimeUtils::CallResult result, std::chrono::nanoseconds) {
				if (TimeUtils::IsEnd(result)) return true;

				auto player = RE::PlayerCharacter::GetSingleton();
				if (player && player->Is3DLoaded() && player->GetParentCell() && player->GetParentCell()->IsAttached()) {
					GetSingleton()->PostLoadData();
					return false;
				}

				return true;
			}, true);
		}

	private:
		static void LoadPluginsForms()
		{
			logger::info("Loading Plugins Forms Data...");

			for (const auto& formInfo : pluginForms) {
				*formInfo.formPtr = TESdataHandler->LookupForm(formInfo.formID, formInfo.pluginName.data());
				if (!*formInfo.formPtr && !formInfo.optional) {
					REPORT_AND_FAIL("ERROR: Form \"{}\" not found in \"{}\".", formInfo.pluginName, formInfo.name, formInfo.pluginName);
				}
			}

			logger::info("Loading Plugins Forms Data: DONE");
		}

		static inline void ExtractGameAssets()
		{
			constexpr unsigned char PscBytes[] = {
				#include "RicochetFramework.psc.h"
			};

			constexpr unsigned char PexBytes[] = {
				#include "RicochetFramework.pex.h"
			};

			const std::string_view PscData{ reinterpret_cast<const char*>(PscBytes), sizeof(PscBytes) - 1 };
			const std::string_view PexData{ reinterpret_cast<const char*>(PexBytes), sizeof(PexBytes) - 1 };

			struct AssetEntry
			{
				std::string_view data;
				std::string_view dest;
				bool isSource;
			};

			const std::array<AssetEntry, 2> assets{{
				{ PscData, "Data/Source/Scripts/RicochetFramework.psc", true },
				{ PexData, "Data/Scripts/RicochetFramework.pex", false }
			}};

			for (const auto& asset : assets) {
				if (asset.isSource && !SettingsIni::bGeneral_ExtractScriptSources) {
					TRACE("ExtractGameAssets: Skipping source script '{}' (ExtractScriptSources disabled).", asset.dest);
					continue;
				}
				try {
					const std::size_t srcHash = std::hash<std::string_view>{}(asset.data);
					const std::filesystem::path destPath(asset.dest);
					if (std::filesystem::exists(destPath)) {
						std::ifstream existing(destPath, std::ios::binary);
						if (existing) {
							const std::string destData{
								std::istreambuf_iterator<char>{existing},
								std::istreambuf_iterator<char>{}
							};
							const std::size_t destHash = std::hash<std::string>{}(destData);
							if (srcHash == destHash) {
								TRACE("ExtractGameAssets: Asset '{}' is up-to-date, skipping.", asset.dest);
								continue;
							}
							if (!SettingsIni::bGeneral_OverwriteInvalidScripts) {
								TRACE("ExtractGameAssets: Asset '{}' differs but overwrite is disabled, skipping.", asset.dest);
								continue;
							}
							TRACE("ExtractGameAssets: Asset '{}' differs, replacing.", asset.dest);
						}
					}
					std::filesystem::create_directories(destPath.parent_path());

					std::ofstream out(destPath, std::ios::binary | std::ios::trunc);
					if (!out) {
						logger::error("ExtractGameAssets: Failed to open output stream for '{}'.", asset.dest);
						continue;
					}

					out.write(asset.data.data(), static_cast<std::streamsize>(asset.data.size()));
					if (!out) {
						std::filesystem::remove(destPath);
						logger::error("ExtractGameAssets: Failed to write '{}'.", asset.dest);
						continue;
					}
					TRACE("ExtractGameAssets: Asset '{}' extracted successfully.", asset.dest);
				} catch (const std::exception& e) {
					logger::error("ExtractGameAssets: Exception extracting '{}': {}", asset.dest, e.what());
				}
			}
		}

		static void InitializeProcessRuntimeForms()
		{
			using namespace CoreStructure;

			logger::info("Initialize Process Runtime Forms...");

			auto applyImpactData = [](RE::BGSImpactData* impactData) {
				if (!impactData) return;

				impactData->model = "";
				impactData->data = {};
				impactData->dData = {};
				impactData->padAC = 0;
				impactData->decalTextureSet = nullptr;
				impactData->decalTextureSet2 = nullptr;
				impactData->sound1 = nullptr;
				impactData->sound2 = nullptr;
				impactData->hazard = nullptr;
				impactData->data.resultOverride = RE::ImpactResult::kNone;
			};

			auto applyImpactDataSet = [](RE::BGSImpactDataSet* impactDataSetData, RE::BGSImpactData* impactData) {
				if (!impactDataSetData || !impactData) return;

				std::unordered_set<RE::BGSMaterialType*> uniqueMaterials;

				for (auto* materialType : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSMaterialType>()) {
					if (materialType) uniqueMaterials.insert(materialType);
				}

				for (auto* race : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESRace>()) {
					if (race && race->bloodImpactMaterial) {
						uniqueMaterials.insert(race->bloodImpactMaterial);
					}
				}

				RE::BSTHashMap<const RE::BGSMaterialType*, RE::BGSImpactData*> newMap;
				for (auto* material : uniqueMaterials) {
					newMap.insert({ material, impactData });
				}

				impactDataSetData->impactMap = std::move(newMap);
			};

			auto applyWeapData = [](RE::TESObjectWEAP* weapData, RE::BGSImpactDataSet* impactDataSetData) {
				if (!weapData) return;

				weapData->impactDataSet = impactDataSetData;
				weapData->weaponData.flags.set(RE::TESObjectWEAP::Data::Flag::kNonPlayable);
				weapData->weaponData.animationType.set(RE::WeaponTypes::kBow);
			};

			ModRuntime_ImpactData = FactoryCreateForm<RE::BGSImpactData>();
			applyImpactData(ModRuntime_ImpactData);

			ModRuntime_ImpactDataSet = FactoryCreateForm<RE::BGSImpactDataSet>();
			applyImpactDataSet(ModRuntime_ImpactDataSet, ModRuntime_ImpactData);

			ModRuntime_Weap = FactoryCreateForm<RE::TESObjectWEAP>();
			applyWeapData(ModRuntime_Weap, ModRuntime_ImpactDataSet);

			ModRuntime_ExtraImpactData = FactoryCreateForm<RE::BGSImpactData>();
			applyImpactData(ModRuntime_ExtraImpactData);

			ModRuntime_ExtraImpactDataSet = FactoryCreateForm<RE::BGSImpactDataSet>();
			applyImpactDataSet(ModRuntime_ExtraImpactDataSet, ModRuntime_ExtraImpactData);

			ModRuntime_ExtraWeap = FactoryCreateForm<RE::TESObjectWEAP>();
			applyWeapData(ModRuntime_ExtraWeap, ModRuntime_ExtraImpactDataSet);

			ModRuntime_CloneImpactData = FactoryCreateForm<RE::BGSImpactData>();
			applyImpactData(ModRuntime_CloneImpactData);

			ModRuntime_CloneImpactDataSet = FactoryCreateForm<RE::BGSImpactDataSet>();
			applyImpactDataSet(ModRuntime_CloneImpactDataSet, ModRuntime_CloneImpactData);

			ModRuntime_CloneWeap = FactoryCreateForm<RE::TESObjectWEAP>();
			applyWeapData(ModRuntime_CloneWeap, ModRuntime_CloneImpactDataSet);

			logger::info("Initialize Process Runtime Forms: DONE");
		}

		static inline void ScanArrowRecoveryPerks()
		{
			using namespace CoreStructure;

			logger::info("Scaning Arrow Recovery Perks...");

			for (auto* perk : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::BGSPerk>()) {
				for (auto* perkEntry : perk->perkEntries) {
					if (!perkEntry || perkEntry->GetType() != RE::PERK_ENTRY_TYPE::kEntryPoint) continue;

					RE::BGSEntryPointPerkEntry* entryPoint = (RE::BGSEntryPointPerkEntry*)perkEntry;
					if (!entryPoint || !entryPoint->functionData || !entryPoint->IsEntryPoint(RE::BGSEntryPoint::ENTRY_POINTS::kModRecoverArrowChance)) continue;

					auto* oneValue = static_cast<RE::BGSEntryPointFunctionDataOneValue*>(entryPoint->functionData);
					if (!oneValue) continue;

					float multiplier = 1.0f;
					const char* typeStr = nullptr;

					const auto fnType = entryPoint->entryData.function.get();

					if (fnType == RE::BGSEntryPointPerkEntry::Function::kMultiplyValue) {
						multiplier = oneValue->data;
						typeStr = "Mult";
					} else if (fnType == RE::BGSEntryPointPerkEntry::Function::kSetValue) {
						multiplier = oneValue->data / 33.0f;
						typeStr = "Set";
					} else if (fnType == RE::BGSEntryPointPerkEntry::Function::kAddValue) {
						multiplier = 1.0f + oneValue->data / 33.0f;
						typeStr = "Add";
					}

					if (typeStr && perk->formID == 0x51B12) { // Hunter's Discipline Perk
						TRACE("Perk <\"{}\" [{:08X}]> affects Arrow Recovery Rate | Type: ({}) | Mult: {:.2f}", perk->GetName(), perk->formID, typeStr, multiplier);
						recoverChancePerkMappings[entryPoint] = multiplier;
						entryPoint->entryData.function = RE::BGSEntryPointPerkEntry::Function::kMultiplyValue;
						oneValue->data = 1.0f;
					} else {
						entryPoint->ClearData();
					}
				}
			}

			logger::info("Scaning Arrow Recovery Perks: DONE");
		}

		template <typename T>
		static T* FactoryCreateForm()
		{
			const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<T>();
			auto form = factory ? factory->Create() : nullptr;
			if (!form) REPORT_AND_FAIL("Failed to initialize mod runtime for type '{}'.", typeid(T).name());

			return form;
		}
	};
}
