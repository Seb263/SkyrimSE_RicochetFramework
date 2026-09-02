#pragma once

#include "DataHandler.hpp"

namespace SettingsIni
{
	// Initialization & default values
	inline int iGeneral_VerboseMode = 1;
	inline bool bGeneral_AsynchronousStartup = false;
	inline bool bGeneral_ShouldIgnoreMaintenanceChecks = false;
	inline bool bGeneral_OverwriteInvalidScripts = true;
	inline bool bGeneral_ExtractScriptSources = false;

	// Global
	inline float fGlobal_ProjectileSpeedMult = 1.0f;
	inline float fGlobal_ProjectileGravityMult = 1.0f;
	inline float fGlobal_ProjectileRangeMult = 1.0f;
	inline float fGlobal_ProjectileForceMult = 1.0f;
	inline int iGlobal_MaxWorldArrows = 32;
	inline int iGlobal_MaxStickyActorArrows = 8;

	// Ricochet
	inline bool bRicochet_Status = true;
	inline float fRicochet_MinVelocity = 1400.0f;
	inline float fRicochet_MaxBounceAngle = 0.5f;
	inline float fRicochet_RestitutionMin = 0.15f;
	inline float fRicochet_RestitutionMax = 0.85f;
	inline float fRicochet_RestitutionScale = 0.9f;
	inline float fRicochet_AngleInfluence = 1.2f;
	inline float fRicochet_VelocityDamp = 0.95f;
	inline float fRicochet_ZPenalty = 10.0f;
	inline float fRicochet_GravityScale = 1.2f;
	inline bool bRicochet_DamageByRestitution = true;
	inline float fRicochet_DamageMult = 1.0f;
	inline bool bRicochet_PlayImpactEffect = true;
	inline bool bRicochet_EffectBoundToProjectile = true;
	inline bool bRicochet_PlayImpactSound = true;
	inline bool bRicochet_TrailFade = false;

	// Fracture
	inline bool bFracture_Status = true;
	inline int iFracture_DefaultRecoveryChance = 50;
	inline float fFracture_GlobalMult = 1.0f;
	inline float fFracture_ChanceMin = 0.05f;
	inline float fFracture_ChanceMax = 0.95f;
	inline float fFracture_SurfaceMultHard = 1.0f;
	inline float fFracture_SurfaceMultSoft = 0.75f;
	inline float fFracture_SurfaceMultActor = 0.5f;
	inline float fFracture_VelocityReference = 0.0f;
	inline float fFracture_VelocityMult = 1.2f;
	inline float fFracture_VelocityMin = 1000.0f;
	inline float fFracture_AngleMult = 1.2f;
	inline float fFracture_AngleExponent = 1.5f;
	inline float fFracture_BrokenPriceMult = 0.5f;
	inline float fFracture_BrokenWeightMult = 0.5f;
	inline bool bFracture_PickupBrokenArrows = true;
	inline bool bFracture_PlayFractureSounds = true;
	inline float fFracture_FractureVolume = 1.0f;
	inline bool bFracture_PlayFractureEffects = true;
	inline bool bFracture_AmmoCanBeReforged = true;

	// Impact
	inline bool bImpact_Status = true;
	inline float fImpact_LinearImpulseReboundMult = 1.0f;
	inline float fImpact_LinearImpulseRicochetMult = 1.0f;
	inline float fImpact_LinearImpulseMin = 15.0f;
	inline float fImpact_LinearImpulseMax = 50.0f;
	inline float fImpact_AngularImpulseMult = 1.0f;
	inline float fImpact_AngularRandomValue = 3.0f;
	inline float fImpact_StickyVelocityMin = 1000.0f;
	inline float fImpact_StickyRollVariance = 1.0f;
	inline float fImpact_ForcedArrowMass = 5.0f;

	// Retrieval
	inline bool bRetrieve_Status = true;
	inline int iRetrieve_Type = 1;
	inline float fRetrieve_SearchRadius = 512.0f;
	inline float fRetrieve_SearchRadiusCombat = 256.0f;

	// Language
	using LanguageMap = std::unordered_map<std::string, std::unordered_map<std::string, std::string>>;
	static inline LanguageMap languageSettingsDefault = {
		{ "BrokenAmmo", {
			{ "english", "%s [broken]" },
			{ "french", "%s [brisée]" },
			{ "german", "%s [zerbrochen]" },
			{ "spanish", "%s [roto]" },
			{ "italian", "%s [spezzata]" },
			{ "polish", "%s [połamana]" },
			{ "russian", "%s [сломанная]" },
			{ "japanese", "%s [壊れた矢]" },
			{ "chinese", "%s [损坏的箭]" }
		}}
	};
	inline LanguageMap languageSettings = languageSettingsDefault;

	class SettingsManager
	{
	public:
		static SettingsManager& GetSingleton()
		{
			static SettingsManager instance;
			return instance;
		}

		SettingsManager()
		{
			bindings = {
				// General
				{ "General", "iVerboseMode", &iGeneral_VerboseMode },
				{ "General", "bAsynchronousStartup", &bGeneral_AsynchronousStartup },
				{ "General", "bShouldIgnoreMaintenanceChecks", &bGeneral_ShouldIgnoreMaintenanceChecks },
				{ "General", "bOverwriteInvalidScripts", &bGeneral_OverwriteInvalidScripts },
				{ "General", "bExtractScriptSources", &bGeneral_ExtractScriptSources },

				// Global
				{ "Global", "fProjectileSpeedMult", &fGlobal_ProjectileSpeedMult },
				{ "Global", "fProjectileGravityMult", &fGlobal_ProjectileGravityMult },
				{ "Global", "fProjectileRangeMult", &fGlobal_ProjectileRangeMult },
				{ "Global", "fProjectileForceMult", &fGlobal_ProjectileForceMult },
				{ "Global", "iMaxWorldArrows", &iGlobal_MaxWorldArrows },
				{ "Global", "iMaxStickyActorArrows", &iGlobal_MaxStickyActorArrows },

				// Ricochet
				{ "Ricochet", "bStatus", &bRicochet_Status },
				{ "Ricochet", "fMinVelocity", &fRicochet_MinVelocity },
				{ "Ricochet", "fMaxBounceAngle", &fRicochet_MaxBounceAngle },
				{ "Ricochet", "fRestitutionMin", &fRicochet_RestitutionMin },
				{ "Ricochet", "fRestitutionMax", &fRicochet_RestitutionMax },
				{ "Ricochet", "fRestitutionScale", &fRicochet_RestitutionScale },
				{ "Ricochet", "fAngleInfluence", &fRicochet_AngleInfluence },
				{ "Ricochet", "fVelocityDamp", &fRicochet_VelocityDamp },
				{ "Ricochet", "fZPenalty", &fRicochet_ZPenalty },
				{ "Ricochet", "fGravityScale", &fRicochet_GravityScale },
				{ "Ricochet", "bDamageByRestitution", &bRicochet_DamageByRestitution },
				{ "Ricochet", "fDamageMult", &fRicochet_DamageMult },
				{ "Ricochet", "bPlayImpactEffect", &bRicochet_PlayImpactEffect },
				{ "Ricochet", "bEffectBoundToProjectile", &bRicochet_EffectBoundToProjectile },
				{ "Ricochet", "bPlayImpactSound", &bRicochet_PlayImpactSound },
				{ "Ricochet", "bTrailFade", &bRicochet_TrailFade },

				// Fracture
				{ "Fracture", "bStatus", &bFracture_Status },
				{ "Fracture", "iDefaultRecoveryChance", &iFracture_DefaultRecoveryChance },
				{ "Fracture", "fGlobalMult", &fFracture_GlobalMult },
				{ "Fracture", "fChanceMin", &fFracture_ChanceMin },
				{ "Fracture", "fChanceMax", &fFracture_ChanceMax },
				{ "Fracture", "fSurfaceMultHard", &fFracture_SurfaceMultHard },
				{ "Fracture", "fSurfaceMultSoft", &fFracture_SurfaceMultSoft },
				{ "Fracture", "fSurfaceMultActor", &fFracture_SurfaceMultActor },
				{ "Fracture", "fVelocityReference", &fFracture_VelocityReference },
				{ "Fracture", "fVelocityMult", &fFracture_VelocityMult },
				{ "Fracture", "fVelocityMin", &fFracture_VelocityMin },
				{ "Fracture", "fAngleMult", &fFracture_AngleMult },
				{ "Fracture", "fAngleExponent", &fFracture_AngleExponent },
				{ "Fracture", "bPickupBrokenArrows", &bFracture_PickupBrokenArrows },
				{ "Fracture", "bPlayFractureSounds", &bFracture_PlayFractureSounds },
				{ "Fracture", "fFractureVolume", &fFracture_FractureVolume },
				{ "Fracture", "bPlayFractureEffects", &bFracture_PlayFractureEffects },
				{ "Fracture", "bAmmoCanBeReforged", &bFracture_AmmoCanBeReforged },
				{ "Fracture", "fBrokenPriceMult", &fFracture_BrokenPriceMult },
				{ "Fracture", "fBrokenWeightMult", &fFracture_BrokenWeightMult },

				// Impact
				{ "Impact", "bStatus", &bImpact_Status },
				{ "Impact", "fLinearImpulseReboundMult", &fImpact_LinearImpulseReboundMult },
				{ "Impact", "fLinearImpulseRicochetMult", &fImpact_LinearImpulseRicochetMult },
				{ "Impact", "fLinearImpulseMin", &fImpact_LinearImpulseMin },
				{ "Impact", "fLinearImpulseMax", &fImpact_LinearImpulseMax },
				{ "Impact", "fAngularImpulseMult", &fImpact_AngularImpulseMult },
				{ "Impact", "fAngularRandomValue", &fImpact_AngularRandomValue },
				{ "Impact", "fStickyVelocityMin", &fImpact_StickyVelocityMin },
				{ "Impact", "fStickyRollVariance", &fImpact_StickyRollVariance },
				{ "Impact", "fForcedArrowMass", &fImpact_ForcedArrowMass },

				// Retrieval
				{ "Retrieval", "bStatus", &bRetrieve_Status },
				{ "Retrieval", "iType", &iRetrieve_Type },
				{ "Retrieval", "fSearchRadius", &fRetrieve_SearchRadius },
				{ "Retrieval", "fSearchRadiusCombat", &fRetrieve_SearchRadiusCombat }
			};

			for (auto& bind : bindings) {
				std::visit([&](auto* ptr) {
					bind.defaultValue = *ptr;
				}, bind.var);
			}
		}

		bool ReadSettings()
		{
			std::wstring   wpath_str(path.begin(), path.end());
			const wchar_t* wpath = wpath_str.c_str();

			bool readStatus = false;

			logger::info("Trying to read INI file at path: {}", path);

			if (std::filesystem::exists(path)) {
				CSimpleIniA ini;
				ini.SetUnicode();

				if (ini.LoadFile(wpath) >= 0) {
					for (const auto& bind : bindings) {
						std::visit([&](auto* ptr) {
							using T = std::decay_t<decltype(*ptr)>;
							if constexpr (std::is_same_v<T, bool>) {
								*ptr = ini.GetBoolValue(bind.section, bind.key, *ptr);
							} else if constexpr (std::is_same_v<T, int>) {
								*ptr = static_cast<int>(ini.GetLongValue(bind.section, bind.key, *ptr));
							} else if constexpr (std::is_same_v<T, float>) {
								*ptr = static_cast<float>(ini.GetDoubleValue(bind.section, bind.key, *ptr));
							} else if constexpr (std::is_same_v<T, std::string>) {
								*ptr = ini.GetValue(bind.section, bind.key, ptr->c_str());
							}
						}, bind.var);
					}
					LoadLanguageSettings(ini);
					readStatus = true;
				} else {
					logger::error("Failed to load INI file at {}", path);
				}
			} else {
				logger::warn("INI file does not exist at {}", path);
			}

			// Clamping logic

			// General
			iGeneral_VerboseMode = std::clamp(iGeneral_VerboseMode, 0, 2);

			// Global
			fGlobal_ProjectileSpeedMult = std::clamp(fGlobal_ProjectileSpeedMult, 0.3f, 5.0f);
			fGlobal_ProjectileGravityMult = std::clamp(fGlobal_ProjectileGravityMult, 0.3f, 5.0f);
			fGlobal_ProjectileRangeMult = std::clamp(fGlobal_ProjectileRangeMult, 0.3f, 5.0f);
			fGlobal_ProjectileForceMult = std::clamp(fGlobal_ProjectileForceMult, 0.0f, 5.0f);
			iGlobal_MaxWorldArrows = std::clamp(iGlobal_MaxWorldArrows, 10, 128);
			iGlobal_MaxStickyActorArrows = std::clamp(iGlobal_MaxStickyActorArrows, 1, 64);

			// Ricochet
			fRicochet_MinVelocity = std::clamp(fRicochet_MinVelocity, 200.0f, 5000.0f);
			fRicochet_MaxBounceAngle = std::clamp(fRicochet_MaxBounceAngle, 0.0f, 1.0f);
			fRicochet_RestitutionMin = std::clamp(fRicochet_RestitutionMin, 0.0f, 1.0f);
			fRicochet_RestitutionMax = std::clamp(fRicochet_RestitutionMax, 0.0f, 1.0f);
			fRicochet_RestitutionScale = std::clamp(fRicochet_RestitutionScale, 0.0f, 1.0f);
			fRicochet_AngleInfluence = std::clamp(fRicochet_AngleInfluence, 0.0f, 5.0f);
			fRicochet_VelocityDamp = std::clamp(fRicochet_VelocityDamp, 0.0f, 1.0f);
			fRicochet_ZPenalty = std::clamp(fRicochet_ZPenalty, 0.0f, 100.0f);
			fRicochet_GravityScale = std::clamp(fRicochet_GravityScale, 0.0f, 5.0f);
			fRicochet_DamageMult = std::clamp(fRicochet_DamageMult, 0.0f, 5.0f);

			// Fracture
			iFracture_DefaultRecoveryChance = std::clamp(iFracture_DefaultRecoveryChance, 0, 100);
			fFracture_GlobalMult = std::clamp(fFracture_GlobalMult, 0.0f, 5.0f);
			fFracture_ChanceMin = std::clamp(fFracture_ChanceMin, 0.0f, 1.0f);
			fFracture_ChanceMax = std::clamp(fFracture_ChanceMax, 0.0f, 1.0f);
			fFracture_SurfaceMultHard = std::clamp(fFracture_SurfaceMultHard, 0.0f, 5.0f);
			fFracture_SurfaceMultSoft = std::clamp(fFracture_SurfaceMultSoft, 0.0f, 5.0f);
			fFracture_SurfaceMultActor = std::clamp(fFracture_SurfaceMultActor, 0.0f, 5.0f);
			fFracture_VelocityReference = std::clamp(fFracture_VelocityReference, 0.0f, 10000.0f);
			fFracture_VelocityMult = std::clamp(fFracture_VelocityMult, 0.0f, 5.0f);
			fFracture_VelocityMin = std::clamp(fFracture_VelocityMin, 0.0f, 5000.0f);
			fFracture_AngleMult = std::clamp(fFracture_AngleMult, 0.0f, 10.0f);
			fFracture_AngleExponent = std::clamp(fFracture_AngleExponent, 0.0f, 10.0f);
			fFracture_FractureVolume = std::clamp(fFracture_FractureVolume, 0.0f, 1.0f);
			fFracture_BrokenPriceMult = std::clamp(fFracture_BrokenPriceMult, 0.0f, 1.0f);
			fFracture_BrokenWeightMult = std::clamp(fFracture_BrokenWeightMult, 0.0f, 1.0f);

			// Impact
			fImpact_LinearImpulseReboundMult = std::clamp(fImpact_LinearImpulseReboundMult, 0.0f, 5.0f);
			fImpact_LinearImpulseRicochetMult = std::clamp(fImpact_LinearImpulseRicochetMult, 0.0f, 5.0f);
			fImpact_LinearImpulseMin = std::clamp(fImpact_LinearImpulseMin, 0.0f, 100.0f);
			fImpact_LinearImpulseMax = std::clamp(fImpact_LinearImpulseMax, 0.0f, 100.0f);
			fImpact_AngularImpulseMult = std::clamp(fImpact_AngularImpulseMult, 0.0f, 5.0f);
			fImpact_AngularRandomValue = std::clamp(fImpact_AngularRandomValue, 0.0f, 10.0f);
			fImpact_StickyVelocityMin = std::clamp(fImpact_StickyVelocityMin, 0.0f, 5000.0f);
			fImpact_StickyRollVariance = std::clamp(std::abs(fImpact_StickyRollVariance), 0.0f, 1.0f);
			fImpact_ForcedArrowMass = std::clamp(fImpact_ForcedArrowMass, 0.0f, 10.0f);

			// Retrieval
			iRetrieve_Type = std::clamp(iRetrieve_Type, 0, 2);
			fRetrieve_SearchRadius = std::clamp(fRetrieve_SearchRadius, 0.0f, 8192.0f);
			fRetrieve_SearchRadiusCombat = std::clamp(fRetrieve_SearchRadiusCombat, 0.0f, 8192.0f);

			// External data
			[&]() {
				using namespace ModData;
				debugVerboseMode = iGeneral_VerboseMode;
			}();

			return readStatus;
		}

		void LoadLanguageSettings(const CSimpleIniA& ini)
		{
			languageSettings = languageSettingsDefault;

			CSimpleIniA::TNamesDepend keys;
			ini.GetAllKeys("Language", keys);

			for (auto& key : keys) {
				std::string keyStr = key.pItem;

				size_t pos = keyStr.rfind('_');
				if (pos == std::string::npos) continue;

				std::string variable = keyStr.substr(0, pos);
				std::string lang = keyStr.substr(pos + 1);
				std::transform(lang.begin(), lang.end(), lang.begin(), ::tolower);

				const char* value = ini.GetValue("Language", keyStr.c_str(), nullptr);
				if (value && *value) languageSettings[variable][lang] = value;
			}
		}

		std::string GetLanguageValue(const std::string& variable, const std::string& lang, const std::string& defaultValue = "") const
		{
			std::string langLower = lang;
			std::transform(langLower.begin(), langLower.end(), langLower.begin(), ::tolower);

			auto varIt = languageSettings.find(variable);
			if (varIt == languageSettings.end()) return defaultValue;

			auto langIt = varIt->second.find(langLower);
			if (langIt == varIt->second.end()) return defaultValue;

			return langIt->second;
		}

		std::optional<std::variant<bool, int, float, std::string>> GetValueVariant(const std::string& key_section)
		{
			auto sep = key_section.rfind(':');
			if (sep == std::string::npos) {
				logger::error("GetValueVariant: Invalid key_section format: '{}'", key_section);
				return std::nullopt;
			}
			std::string section = key_section.substr(0, sep);
			std::string key     = key_section.substr(sep + 1);
			for (const auto& bind : bindings) {
				if (key == bind.key && section == bind.section) {
					if (auto v = std::get_if<bool*>        (&bind.var)) return **v;
					if (auto v = std::get_if<int*>         (&bind.var)) return **v;
					if (auto v = std::get_if<float*>       (&bind.var)) return **v;
					if (auto v = std::get_if<std::string*> (&bind.var)) return **v;
				}
			}
			return std::nullopt;
		}

		template <typename T>
		T GetValue(const std::string& key_section, const T& defaultValue = T{})
		{
			auto opt = GetValueVariant(key_section);
			if (!opt) {
				logger::error("GetValue: No binding found for '{}'", key_section);
				return defaultValue;
			}

			return std::visit([&](auto&& val) -> T {
				using V = std::decay_t<decltype(val)>;
				if constexpr (std::is_same_v<T, std::string>) {
					if constexpr (std::is_same_v<V, std::string>) return val;
				} else if constexpr (std::is_same_v<T, bool>) {
					if constexpr (std::is_same_v<V, bool>)  return val;
					if constexpr (std::is_same_v<V, int>)   return val != 0;
					if constexpr (std::is_same_v<V, float>) return val != 0.0f;
				} else if constexpr (std::is_same_v<T, int>) {
					if constexpr (std::is_same_v<V, int>)   return val;
					if constexpr (std::is_same_v<V, float>) return static_cast<int>(val);
					if constexpr (std::is_same_v<V, bool>)  return val ? 1 : 0;
				} else if constexpr (std::is_same_v<T, float>) {
					if constexpr (std::is_same_v<V, float>) return val;
					if constexpr (std::is_same_v<V, int>)   return static_cast<float>(val);
					if constexpr (std::is_same_v<V, bool>)  return val ? 1.0f : 0.0f;
				}
				logger::error("GetValue: Type mismatch for '{}'", key_section);
				return defaultValue;
			}, *opt);
		}

		std::optional<std::variant<bool, int, float, std::string>> GetDefaultValueVariant(const std::string& key_section)
		{
			auto sep = key_section.rfind(':');
			if (sep == std::string::npos) {
				logger::error("GetDefaultValueVariant: Invalid key_section format: '{}'", key_section);
				return std::nullopt;
			}
			std::string section = key_section.substr(0, sep);
			std::string key = key_section.substr(sep + 1);

			for (const auto& bind : bindings) {
				if (key == bind.key && section == bind.section) {
					return bind.defaultValue;
				}
			}
			return std::nullopt;
		}

		template <typename T>
		T GetDefaultValue(const std::string& key_section, const T& fallback = T{})
		{
			auto opt = GetDefaultValueVariant(key_section);
			if (!opt) {
				logger::error("GetDefaultValue: No binding found for '{}'", key_section);
				return fallback;
			}

			return std::visit([&](auto&& val) -> T {
				using V = std::decay_t<decltype(val)>;
				if constexpr (std::is_same_v<T, bool>) {
					if constexpr (std::is_same_v<V, bool>)  return val;
					if constexpr (std::is_same_v<V, int>)   return val != 0;
					if constexpr (std::is_same_v<V, float>) return val != 0.0f;
				} else if constexpr (std::is_same_v<T, int>) {
					if constexpr (std::is_same_v<V, int>)   return val;
					if constexpr (std::is_same_v<V, float>) return static_cast<int>(val);
					if constexpr (std::is_same_v<V, bool>)  return val ? 1 : 0;
				} else if constexpr (std::is_same_v<T, float>) {
					if constexpr (std::is_same_v<V, float>) return val;
					if constexpr (std::is_same_v<V, int>)   return static_cast<float>(val);
					if constexpr (std::is_same_v<V, bool>)  return val ? 1.0f : 0.0f;
				} else if constexpr (std::is_same_v<T, std::string>) {
					if constexpr (std::is_same_v<V, std::string>) return val;
				}
				logger::error("GetDefaultValue: Type mismatch for '{}'", key_section);
				return fallback;
			}, *opt);
		}

		template <typename T>
		bool SetValue(const std::string& key_section, const T& value)
		{
			auto sep = key_section.rfind(':');
			if (sep == std::string::npos) {
				logger::error("SetValue: Invalid key_section format: '{}'", key_section);
				return false;
			}

			std::string section = key_section.substr(0, sep);
			std::string key = key_section.substr(sep + 1);

			if (section.empty() || key.empty()) {
				logger::error("SetValue: Empty section or key in '{}'", key_section);
				return false;
			}

			for (auto& bind : bindings) {
				if (section == bind.section && key == bind.key) {
					bool matched = std::visit([&](auto* ptr) -> bool {
						using PtrType = std::decay_t<decltype(*ptr)>;
						if constexpr (std::is_same_v<PtrType, T>) {
							*ptr = value;
							return true;
						}
						return false;
					}, bind.var);

					if (!matched) {
						logger::error("SetValue: Type mismatch for '{}:{}'", section, key);
						return false;
					}

					CSimpleIniA ini;
					ini.SetUnicode();
					if (std::filesystem::exists(path)) ini.LoadFile(path.c_str());

					if constexpr (std::is_same_v<T, bool>) {
						ini.SetBoolValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, int>) {
						ini.SetLongValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, float>) {
						ini.SetDoubleValue(section.c_str(), key.c_str(), value);
					} else if constexpr (std::is_same_v<T, std::string>) {
						ini.SetValue(section.c_str(), key.c_str(), value.c_str());
					} else {
						return false;
					}

					if (ini.SaveFile(path.c_str()) < 0) {
						logger::error("SetValue: Failed to save INI file at '{}'", path);
						return false;
					}

					return true;
				}
			}

			logger::error("SetValue: No binding found for '{}:{}'", section, key);
			return false;
		}

	private:
		inline static std::string path = "Data/SKSE/Plugins/RicochetFramework.ini";
		inline static std::string prefix = "RF";

		using IniValue = std::variant<bool*, int*, float*, std::string*>;

		struct IniBinding
		{
			const char* section;
			const char* key;
			IniValue var;
			bool syncGlobal = false;
			std::variant<bool, int, float, std::string> defaultValue;
		};

		std::vector<IniBinding> bindings;
	};
}
