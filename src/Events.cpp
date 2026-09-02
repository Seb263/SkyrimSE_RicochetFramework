#include "Events.h"

namespace Events
{
	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESLoadGameEvent* event, RE::BSTEventSource<RE::TESLoadGameEvent>*)
	{
		ModData::lastLoadPoint = std::chrono::steady_clock::now();

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const SKSE::CrosshairRefEvent* event, RE::BSTEventSource<SKSE::CrosshairRefEvent>*)
	{
		using namespace ModData;

		crosshairRefProjectile = MiscUtils::ResolveHandle<RE::Projectile>(event->crosshairRef);
		if (crosshairRefProjectile && !crosshairRefProjectile->IsActivationBlocked()) {
			crosshairRefProjectile->SetActivationBlocked(true);
		}

		return continueEvent;
	}

	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESActivateEvent* event, RE::BSTEventSource<RE::TESActivateEvent>*)
	{
		if (!event || !event->objectActivated || !event->objectActivated.get() || !event->actionRef || !event->actionRef.get()) return continueEvent;

		auto* activatedRef = event->objectActivated->As<RE::Projectile>();
		auto* activatorRef = event->actionRef->As<RE::Actor>();
		if (!activatedRef || !activatorRef) return continueEvent;

		ModCore::Retrieval::ActivateProjectile(activatedRef, activatorRef);

		return continueEvent;
	}
}
