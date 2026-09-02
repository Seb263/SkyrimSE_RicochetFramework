#pragma once

#include "SettingsIni.hpp"

#include "Utils/NiUtils.hpp"

class ModUtils
{
public:

	struct CollisionData
	{
		bool isValid = false;
		RE::hkVector4 hitPosition{};
		RE::hkVector4 hitAngle{};
		RE::TESObjectREFR* hitRef = nullptr;
		RE::hkpCollidable* collidableA = nullptr;
		RE::hkpCollidable* collidableB = nullptr;
	};

	static CollisionData GetCollisionDataFromCollector(RE::hkpAllCdPointCollector* allCdPointCollector, RE::Projectile* projectile)
	{
		CollisionData data{};
		if (!allCdPointCollector) return data;

		auto hitOpt = NiUtils::GetLastHit(allCdPointCollector);
		if (!hitOpt.has_value()) return data;
		const auto& hit = *hitOpt;

		auto [hitRef, collidableA, collidableB] = NiUtils::GetRefAndCollidables(hit, projectile);

		data.isValid = true;
		data.hitPosition = hit.contact.position;
		data.hitAngle = hit.contact.separatingNormal;
		data.hitRef = hitRef;
		data.collidableA = collidableA;
		data.collidableB = collidableB;

		return data;
	}
	
	static bool IsArrowExcluded(RE::Projectile* projectile)
	{
		if (!projectile) return true;
		
		auto* projectileBase = projectile->GetProjectileRuntimeData().ammoSource;
		if (projectileBase && ModData::WeapTypeBoundArrow) {
			if (auto* projectileAsKeyword = projectileBase->AsKeywordForm()) {
				if (projectileAsKeyword->HasKeyword(ModData::WeapTypeBoundArrow)) return true;
			}
		}

		return false;
	}

	template <typename T>
	static RE::MATERIAL_ID GetFirstValidMaterial(const T& collidables) {
		RE::MATERIAL_ID materialID = RE::MATERIAL_ID::kNone;

		auto visit = [&](auto* collidable) {
			NiUtils::VisitShapeMaterials(collidable, [&](RE::MATERIAL_ID fMaterialID) -> bool {
				if (fMaterialID != RE::MATERIAL_ID::kNone) {
					materialID = fMaterialID;
					return false;
				}
				return true;
			});
		};

		if constexpr (std::is_pointer_v<T>) {
			visit(collidables);
		} else {
			for (auto* collidable : collidables) {
				visit(collidable);
				if (materialID != RE::MATERIAL_ID::kNone) break;
			}
		}

		return materialID;
	}

	static bool SpawnParticle(RE::TESObjectREFR* ref, RE::BGSImpactData* impactData, const RE::NiPoint3& position, const RE::NiPoint3& direction, const bool linked = true)
	{
		if (!ref || !impactData) return false;

		RE::NiAVObject* avObject = ref->Get3D(false);
		RE::TESObjectCELL* cell = ref->GetParentCell();
		if (!cell) return false;

		if (!IsPositionInLoadedCell(position)) return false;

		const char* model = impactData->GetModel();
		if (!model) return false;

		return RE::BSTempEffectParticle::Spawn(cell, 3.0f, model, direction, position, 1.0f, 7, linked ? avObject : nullptr);
	}

	static void SpawnSound(RE::BGSImpactData* impactData, RE::NiPoint3& position, RE::Actor* attacker = nullptr)
	{
		if (!impactData || (!impactData->sound1 && !impactData->sound2) || !IsPositionInLoadedCell(position)) return;

		RE::BGSImpactManager::ImpactSoundData soundData{
			.impactData = impactData,
			.position = &position,
			.objectToFollow = nullptr,
			.sound1 = nullptr,
			.sound2 = nullptr,
			.playSound1 = impactData->sound1 ? true : false,
			.playSound2 = impactData->sound2 && attacker && attacker->IsPlayerRef(),
			.lowPriority = false,
			.pool = nullptr
		};
		RE::BGSImpactManager::GetSingleton()->PlayImpactDataSounds(soundData);
	}

	static RE::TESBoundObject* GetAmmoSwap(RE::Projectile* projectile)
	{
		if (!projectile) return nullptr;

		auto& cloneExtra = projectile->extraList;
		if (!&cloneExtra) return nullptr;

		auto* cloneAttached = cloneExtra.GetByType<RE::ExtraPromotedRef>();
		if (!cloneAttached) return nullptr;

		for (auto& owner : cloneAttached->promotedRefOwners) {
			if (auto* boundObj = owner->As<RE::TESBoundObject>()) {
				return boundObj;
			}
		}

		return nullptr;
	}

	static bool IsPositionInLoadedCell(const RE::NiPoint3& position)
	{
		auto* tes = RE::TES::GetSingleton();
		if (!tes) return false;

		if (auto* cell = tes->interiorCell; cell) {
			return cell->IsAttached();
		}

		if (const auto gridLength = tes->gridCells ? tes->gridCells->length : 0; gridLength > 0) {
			const std::int32_t targetGridX = static_cast<std::int32_t>(std::floor(position.x / 4096.0f));
			const std::int32_t targetGridY = static_cast<std::int32_t>(std::floor(position.y / 4096.0f));

			for (std::uint32_t x = 0; x < gridLength; ++x) {
				for (std::uint32_t y = 0; y < gridLength; ++y) {
					auto* cell = tes->gridCells->GetCell(x, y);
					if (!cell || !cell->IsAttached()) continue;

					const auto* coords = cell->GetCoordinates();
					if (!coords) continue;

					if (coords->cellX == targetGridX && coords->cellY == targetGridY) {
						return true;
					}
				}
			}
		}

		return false;
	}
};
