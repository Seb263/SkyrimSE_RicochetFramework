#pragma once

namespace ModData
{
	constexpr std::string_view MOD_NAME = "Ricochet Framework";

	inline auto lastLoadPoint = std::chrono::steady_clock::now();

	struct PluginForm
	{
		std::string_view name;
		void** formPtr;
		uint32_t formID;
		std::string_view pluginName;
		bool optional = false;
	};

	// Properties storing game form references
	inline RE::BGSKeyword* WeapTypeBoundArrow;

	static inline const std::vector<PluginForm> pluginForms = {
		{ "WeapTypeBoundArrow", reinterpret_cast<void**>(&WeapTypeBoundArrow), 0x10D501, "Skyrim.esm", true }
	};

	inline RE::Projectile* crosshairRefProjectile;
	inline RE::TESDataHandler* TESdataHandler;
	inline std::string gameLanguage;
}
