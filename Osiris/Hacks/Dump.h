#pragma once
#include "../SDK/GameEvent.h"
namespace Dump {
    void dump() noexcept;
    void dump_player_death(GameEvent& event) noexcept;
    void dump_event_userid(GameEvent& event, const char* event_name) noexcept;
}