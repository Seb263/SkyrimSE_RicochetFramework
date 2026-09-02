#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Structure.h"

#include "Utils/JsonUtils.hpp"
#include "Utils/MiscUtils.hpp"
#include "Utils/ModUtils.hpp"

namespace JSONHandler
{
	using namespace ModData;

	class Main
	{
	public:
		static void LoadMappings();

	private:
		static void ProcessAmmoMapping(const json& jsonData);
		static void ProcessFractureMapping(const json& jsonData);
		static void ProcessFractureMappingFilters(const json& filterData, CoreStructure::FractureMappingFilters& tmpFilters);
		static void ProcessFractureMappingModifiers(const json& modifiers, CoreStructure::FractureMappingModifiers& tmpFilters);

		template <typename Type>
		static Type* ResolveForm(const std::string& str, const bool useAssociatedForm = false);

		template <typename Type, typename Output>
		static void ParseMappingFilterFormJson(const json& filters, const std::string& key, std::vector<Output>& out, const bool useAssociatedForm = false);

		template <typename T>
		static void SetFilterValue(const json& filters, const std::string& key, std::optional<std::variant<T, RE::TESGlobal*>>& target);
	};
}
