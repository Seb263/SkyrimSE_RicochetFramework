#pragma once

#include "DataHandler.hpp"

class MiscUtils
{
	public:

	template <typename Container>
	static Container SplitString(const std::string& str, char delimiter, bool removeSpaces = true, bool toLower = false)
	{
		Container tokens;
		std::stringstream ss(str);
		std::string token;

		while (std::getline(ss, token, delimiter)) {
			if (removeSpaces) {
				token.erase(0, token.find_first_not_of(' '));
				token.erase(token.find_last_not_of(' ') + 1);
			}

			if (toLower) {
				std::transform(token.begin(), token.end(), token.begin(), ::tolower);
			}

			if (!token.empty()) {
				if constexpr (std::is_same_v<Container, std::unordered_set<std::string>>) {
					tokens.insert(token);
				} else {
					tokens.push_back(token);
				}
			}
		}
		return tokens;
	}

	inline static std::unordered_map<std::string, RE::TESForm*> g_getFormCache;
	template <typename T>
	static T* GetFormFromAssoc(const std::string& a_string, const bool cacheResult = true)
	{
		if (a_string.empty()) return nullptr;

		if (const auto it = g_getFormCache.find(a_string); it != g_getFormCache.end()) {
			if constexpr (std::is_same_v<T, RE::TESForm>) return it->second;
			else if (auto* typed = it->second->As<T>()) return typed;
			else {
				logger::warn("GetFormFromAssoc: \"{}\" is incompatible with type \"{}\" (cache).", a_string, typeid(T).name());
				return nullptr;
			}
		}

		const std::size_t sep = a_string.find(':');
		if (sep == std::string::npos) {
			logger::warn("GetFormFromAssoc: Invalid format for \"{}\"", a_string);
			return nullptr;
		}

		const std::string hexPart = a_string.substr(sep + 1);
		std::size_t charsRead = 0;
		std::uint32_t formID = 0;

		try {
			formID = std::stoul(hexPart, &charsRead, 16);
		} catch (const std::invalid_argument&) {
			logger::warn("GetFormFromAssoc: Invalid hexadecimal value in \"{}\"", a_string);
			return nullptr;
		} catch (const std::out_of_range&) {
			logger::warn("GetFormFromAssoc: FormID out of range in \"{}\"", a_string);
			return nullptr;
		}

		if (charsRead != hexPart.size()) {
			logger::warn("GetFormFromAssoc: Malformed hexadecimal value in \"{}\" (invalid character at position {})", a_string, charsRead);
			return nullptr;
		}

		auto* base = RE::TESDataHandler::GetSingleton()->LookupForm(static_cast<RE::FormID>(formID), a_string.substr(0, sep));
		if (!base) {
			logger::warn("GetFormFromAssoc: \"{}\" could not be found.", a_string);
			return nullptr;
		}

		if (cacheResult) g_getFormCache[a_string] = base;

		if constexpr (std::is_same_v<T, RE::TESForm>) return base;
		else if (auto* typed = base->As<T>()) return typed;
		else {
			logger::warn("GetFormFromAssoc: \"{}\" is incompatible with type \"{}\".", a_string, typeid(T).name());
			return nullptr;
		}
	}

	inline static std::unordered_map<RE::FormType, std::unordered_map<std::string, RE::TESForm*>> g_getFormEditorCache;
	template <typename T>
	static T* GetFormFromEditorID(const std::string& a_string)
	{
		if (a_string.empty()) return nullptr;

		auto& cache = g_getFormEditorCache[T::FORMTYPE];

		if (cache.empty()) {
			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler) return nullptr;

			auto forms = dataHandler->GetFormArray<T>();
			for (auto* form : forms) {
				if (!form) continue;

				const char* editorID = form->GetFormEditorID();
				if (editorID && *editorID != '\0') cache.emplace(editorID, form);
			}
		}

		auto it = cache.find(a_string);
		if (it == cache.end()) {
			logger::warn("GetFormFromEditorID: \"{}\" not found.", a_string);
			return nullptr;
		}

		if constexpr (std::is_same_v<T, RE::TESForm>) return it->second;
		if (auto* typed = it->second->As<T>()) return typed;

		logger::warn("GetFormFromEditorID: \"{}\" is incompatible with requested type.", a_string);
		return nullptr;
	}

	static void ClearGetFormLookupCache()
	{
		g_getFormCache.clear();
		g_getFormEditorCache.clear();
	}

	template <bool RECURSIVE = false>
	static std::vector<std::string> GetAllFiles(std::string_view a_path = {}, std::string_view a_ext = {},
												std::string_view a_prefix = {}, std::string_view a_suffix = {}) noexcept {
		using dir_iterator = std::conditional_t<RECURSIVE, std::filesystem::recursive_directory_iterator, std::filesystem::directory_iterator>;

		std::vector<std::string> files;

		auto file_iterator = [&](const std::filesystem::directory_entry& a_file) {
			if (a_file.exists() && !a_file.path().empty()) {
				if (!a_ext.empty() && a_file.path().extension() != a_ext) {
					return;
				}

				const auto path = a_file.path().string();

				if ((!a_prefix.empty() && path.find(a_prefix) != std::string::npos) ||
					(!a_suffix.empty() && path.rfind(a_suffix) != std::string::npos) ||
					(a_prefix.empty() && a_suffix.empty())) {
					files.push_back(path);
				}
			}
		};

		std::string dir(MAX_PATH + 1, ' ');
		auto res = GetModuleFileNameA(nullptr, dir.data(), MAX_PATH + 1);
		if (res == 0) REPORT_AND_FAIL("Unable to acquire valid path using default null path argument!\nExpected: Current directory\nResolved: NULL");

		auto eol = dir.find_last_of("\\/");
		dir = dir.substr(0, eol);

		std::filesystem::path path = a_path.empty() ? std::filesystem::path{dir} : std::filesystem::path{a_path};

		if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path)) {
			path = dir / path;
		}

		if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
			for (const auto& entry : dir_iterator(path)) {
				file_iterator(entry);
			}
			std::ranges::sort(files);
		} else {
			logger::warn("Provided path is invalid or not a directory: {}", path.string());
		}

		return files;
	}



	template <typename T = RE::TESObjectREFR, typename HandleT>
	static T* ResolveHandle(const HandleT& handle)
	{
		auto ptr = handle ? handle.get() : nullptr;
		if (!ptr) return nullptr;

		return ptr->As<T>();
	}

	static bool IsFormIDValid(const RE::FormID formID)
	{
		return (formID > 0x0 && formID < 0xFFFFFFFF);
	}

	template <typename T = RE::TESObjectREFR>
	static T* GetValidReference(RE::FormID formID, const bool extraChecks = false)
	{
		if (!MiscUtils::IsFormIDValid(formID)) return nullptr;
		return GetValidReference<T>(RE::TESForm::LookupByID<RE::TESObjectREFR>(formID), extraChecks);
	}

	template <typename T = RE::TESObjectREFR>
	static T* GetValidReference(RE::TESObjectREFR* ref, const bool extraChecks = false)
	{
		using namespace ModData;

		if (!ref || !ref->As<T>() || !MiscUtils::IsFormIDValid(ref->formID) || ref->IsDeleted()) return nullptr;
		
		if (extraChecks) {
			if (ref->IsDisabled() || ref->IsMarkedForDeletion()) return nullptr;
		}

		if constexpr (std::is_same_v<T, RE::Actor>) {
			auto* refActor = ref->As<RE::Actor>();
			if (!refActor || !ref->Is(RE::FormType::ActorCharacter)) return nullptr;
			
			if (extraChecks && (refActor->GetActorRuntimeData().criticalStage != RE::ACTOR_CRITICAL_STAGE::kNone)) return nullptr;
		}

		if constexpr (std::is_same_v<T, RE::Projectile>) {
			auto* refProjectile = ref->As<RE::Projectile>();
			if (!refProjectile) return nullptr;
			
			if (extraChecks && (refProjectile->GetProjectileRuntimeData().flags.any(RE::Projectile::Flags::kDestroyed))) return nullptr;
		}

		return ref->As<T>();
	}

	template <typename T = void>
	static auto ResolveVariantValue(const auto& var)
	{
		if (!var.has_value()) {
			if constexpr (std::is_void_v<T>) {
				using FirstType = std::variant_alternative_t<0, std::decay_t<decltype(*var)>>;
				return FirstType(0);
			} else return T(0);
		}

		const auto& v = *var;
		if constexpr (std::is_void_v<T>) {
			using FirstType = std::variant_alternative_t<0, std::decay_t<decltype(v)>>;
			if (std::holds_alternative<FirstType>(v)) return std::get<FirstType>(v);
			if (auto g = std::get_if<RE::TESGlobal*>(&v); g && *g) {
				if constexpr (std::is_same_v<FirstType, bool>) return (*g)->value != 0.0f;
				else return (*g)->value;
			}
			return FirstType(0);
		} else {
			if (std::holds_alternative<T>(v)) return std::get<T>(v);
			if (auto g = std::get_if<RE::TESGlobal*>(&v); g && *g) {
				if constexpr (std::is_same_v<T, bool>) return (*g)->value != 0.0f;
				else return (*g)->value;
			}
			return T(0);
		}
	}

	static void SetRotationMatrix(RE::NiMatrix3& matrix, const float sinAlphaCosBeta, const float cosAlphaCosBeta, const float sinBeta)
	{
		const float cosBeta = std::sqrtf(1.0f - sinBeta * sinBeta);
		const float cosAlpha = cosAlphaCosBeta / cosBeta;
		const float sinAlpha = sinAlphaCosBeta / cosBeta;

		matrix.entry[0][0] = cosAlpha;
		matrix.entry[0][1] = -sinAlphaCosBeta;
		matrix.entry[0][2] = sinAlpha * sinBeta;

		matrix.entry[1][0] = sinAlpha;
		matrix.entry[1][1] = cosAlphaCosBeta;
		matrix.entry[1][2] = -cosAlpha * sinBeta;

		matrix.entry[2][0] = 0.0f;
		matrix.entry[2][1] = sinBeta;
		matrix.entry[2][2] = cosBeta;
	}

	enum class LocalAxis { Pitch, Yaw, Roll };
	static void ApplyLocalRotation(RE::NiMatrix3& matrix, float angle, LocalAxis axis)
	{
		float c = std::cosf(angle);
		float s = std::sinf(angle);

		RE::NiPoint3 right(matrix.entry[0][0], matrix.entry[0][1], matrix.entry[0][2]);
		RE::NiPoint3 up(matrix.entry[1][0], matrix.entry[1][1], matrix.entry[1][2]);
		RE::NiPoint3 forward(matrix.entry[2][0], matrix.entry[2][1], matrix.entry[2][2]);

		switch (axis) {
		case LocalAxis::Pitch:
			matrix.entry[1][0] = c * up.x - s * forward.x;
			matrix.entry[1][1] = c * up.y - s * forward.y;
			matrix.entry[1][2] = c * up.z - s * forward.z;

			matrix.entry[2][0] = s * up.x + c * forward.x;
			matrix.entry[2][1] = s * up.y + c * forward.y;
			matrix.entry[2][2] = s * up.z + c * forward.z;
			break;

		case LocalAxis::Yaw:
			matrix.entry[0][0] = c * right.x + s * forward.x;
			matrix.entry[0][1] = c * right.y + s * forward.y;
			matrix.entry[0][2] = c * right.z + s * forward.z;

			matrix.entry[2][0] = -s * right.x + c * forward.x;
			matrix.entry[2][1] = -s * right.y + c * forward.y;
			matrix.entry[2][2] = -s * right.z + c * forward.z;
			break;

		case LocalAxis::Roll:
			matrix.entry[0][0] = c * right.x + s * up.x;
			matrix.entry[0][1] = c * right.y + s * up.y;
			matrix.entry[0][2] = c * right.z + s * up.z;

			matrix.entry[1][0] = -s * right.x + c * up.x;
			matrix.entry[1][1] = -s * right.y + c * up.y;
			matrix.entry[1][2] = -s * right.z + c * up.z;
			break;
		}
	}

	static RE::NiPoint3 HkVector4ToNiPoint3(const RE::hkVector4& vec)
	{
		const float x = _mm_cvtss_f32(vec.quad);
		const float y = _mm_cvtss_f32(_mm_shuffle_ps(vec.quad, vec.quad, _MM_SHUFFLE(1, 1, 1, 1)));
		const float z = _mm_cvtss_f32(_mm_shuffle_ps(vec.quad, vec.quad, _MM_SHUFFLE(2, 2, 2, 2)));

		return RE::NiPoint3(x, y, z);
	}

	static RE::Projectile::ProjectileRot DirToAngles(const RE::NiPoint3& dir, const bool invert = false)
	{
		RE::Projectile::ProjectileRot rot{};
		RE::NiPoint3 norm = dir;
		norm.Unitize();

		if (!invert) {
			rot.x = -asinf(norm.z);
			rot.z = atan2f(norm.x, norm.y);
		} else {
			rot.x = asinf(norm.z);
			rot.z = atan2f(-norm.x, -norm.y);
		}

		return rot;
	}

	static float AbsSum(const RE::NiPoint3 value)
	{
		return std::fabs(value.x) + std::fabs(value.y) + std::fabs(value.z);
	}

	static RE::NiPoint3 Normalized(const RE::NiPoint3 value)
	{
		RE::NiPoint3 tmp = value;
		tmp.Unitize();
		return tmp;
	}

	static RE::NiPoint3 ClampNiPoint3(const RE::NiPoint3& value, const float minVal, const float maxVal)
	{
		float length = value.Length();
		if (length < 1e-6f) return RE::NiPoint3();

		float clampedLength = std::clamp(length, minVal, maxVal);
		return Normalized(value) * clampedLength;
	}

	static float GetRandomNumber(float min = 0.0f, float max = 1.0f)
	{
		static std::mt19937 generator(std::random_device{}());
		std::uniform_real_distribution<float> distribution(min, max);
		return distribution(generator);
	}
};
