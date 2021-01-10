#include <cassert>

#include "EventListener.h"
#include "fnv.h"
#include "Hacks/Misc.h"
#include "Hacks/SkinChanger.h"
#include "Hacks/Visuals.h"
#include "Interfaces.h"
#include "Memory.h"
#include "Hacks/Dump.h"
#include "Config.h"
EventListener::EventListener() noexcept
{
    assert(interfaces);

    interfaces->gameEventManager->addListener(this, "item_purchase");
    interfaces->gameEventManager->addListener(this, "round_start");
    interfaces->gameEventManager->addListener(this, "round_freeze_end");
    interfaces->gameEventManager->addListener(this, "player_hurt");
    interfaces->gameEventManager->addListener(this, "weapon_fire");
    interfaces->gameEventManager->addListener(this, "weapon_reload");
    interfaces->gameEventManager->addListener(this, "weapon_zoom");
    interfaces->gameEventManager->addListener(this, "player_spawned");
    interfaces->gameEventManager->addListener(this, "player_jump");

    interfaces->gameEventManager->addListener(this, "player_death");
    

    if (const auto desc = memory->getEventDescriptor(interfaces->gameEventManager, "player_death", nullptr))
        std::swap(desc->listeners[0], desc->listeners[desc->listeners.size - 1]);
    else
        assert(false);
}

void EventListener::remove() noexcept
{
    assert(interfaces);

    interfaces->gameEventManager->removeListener(this);
}

void EventListener::fireGameEvent(GameEvent* event)
{;
    switch (fnv::hashRuntime(event->getName())) {
    case fnv::hash("round_start"):
    case fnv::hash("item_purchase"):
    case fnv::hash("round_freeze_end"):
        Misc::purchaseList(event);
        break;
    case fnv::hash("player_death"):
        SkinChanger::updateStatTrak(*event);
        SkinChanger::overrideHudIcon(*event);
        Misc::killMessage(*event);
        Misc::killSound(*event);
        Dump::dump_player_death(*event);
        break;
    case fnv::hash("player_hurt"):
        Misc::playHitSound(*event);
        Visuals::hitEffect(event);
        Visuals::hitMarker(event);
        break;
    case fnv::hash("weapon_fire"):
        Dump::dump_event_userid(*event, "weapon_fire");
        break;
    case fnv::hash("weapon_reload"):
        Dump::dump_event_userid(*event, "weapon_reload");
        break;
    case fnv::hash("weapon_zoom"):
        Dump::dump_event_userid(*event, "weapon_zoom");
        break;
    case fnv::hash("player_spawned"):
        Dump::dump_event_userid(*event, "player_spawned");
        break;
    case fnv::hash("player_jump"):
        Dump::dump_event_userid(*event, "player_jump");
        break;
    }
}
