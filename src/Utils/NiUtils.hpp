#pragma once

class NiUtils
{

public:

	static bool IsReferenceRagdollReady(RE::TESObjectREFR* ref)
	{
		if (!ref || !ref->Is3DLoaded()) return false;

		RE::NiAVObject* niAVObject = ref->Get3D();
		if (!niAVObject) return false;
		
		auto* hkpRigidBody = GetRigidBody(niAVObject);
		if (hkpRigidBody && hkpRigidBody->world && hkpRigidBody->motion.GetMass() > 0.0f) return true;

		return false;
	}

	static RE::hkpRigidBody* GetRigidBody(RE::NiAVObject* a_object)
	{
		if (!a_object) return nullptr;

		const auto collisionObject = a_object->GetCollisionObject();
		if (!collisionObject) return nullptr;

		const auto bhkRigidBody = RE::NiPointer<RE::bhkRigidBody>(collisionObject->GetRigidBody());
		if (!bhkRigidBody || !bhkRigidBody->referencedObject) return nullptr;

		const auto hkpRigidBody = static_cast<RE::hkpRigidBody*>(bhkRigidBody->referencedObject.get());
		return hkpRigidBody;
	}

	static bool VisitShapeMaterials(RE::hkpCollidable* collidable, std::function<bool(RE::MATERIAL_ID)> func)
	{
		if (!collidable || !collidable->shape) return true;

		if (const auto* hkpShape = collidable ? collidable->shape : nullptr) {
			if (hkpShape->type == RE::hkpShapeType::kMOPP) {
				const auto* mopp = static_cast<const RE::hkpMoppBvTreeShape*>(hkpShape);
				const auto* childShape = mopp ? mopp->child.childShape : nullptr;
				const auto* compressedShape = childShape ? netimmerse_cast<RE::bhkCompressedMeshShape*>(childShape->userData) : nullptr;
				const auto* shapeData = compressedShape ? compressedShape->data.get() : nullptr;

				if (shapeData) {
					for (auto& meshMaterial : shapeData->meshMaterials) {
						if (!func(meshMaterial.materialID)) return false;
					}
				}
			} else if (const auto* bhkShape = hkpShape->userData; bhkShape) {
				if (!func(bhkShape->materialID)) return false;
			}
		}

		return true;
	}

	static std::optional<RE::hkpRootCdPoint> GetLastHit(RE::hkpAllCdPointCollector* allCdPointCollector)
	{
		if (!allCdPointCollector || allCdPointCollector->hits.empty()) return std::nullopt;

		return allCdPointCollector->hits.back();
	}

	static std::tuple<RE::TESObjectREFR*, RE::hkpCollidable*, RE::hkpCollidable*> GetRefAndCollidables(const RE::hkpRootCdPoint& hit, RE::Projectile* projectile)
	{
		RE::TESObjectREFR* ref = nullptr;

		if (hit.rootCollidableA) {
			if (auto* r = RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableA)) {
				if (projectile != r) ref = r;
			}
		}
		if (!ref && hit.rootCollidableB) {
			if (auto* r = RE::TESHavokUtilities::FindCollidableRef(*hit.rootCollidableB)) {
				if (projectile != r) ref = r;
			}
		}

		return { ref, const_cast<RE::hkpCollidable*>(hit.rootCollidableA), const_cast<RE::hkpCollidable*>(hit.rootCollidableB) };
	}
};
