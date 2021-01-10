#define NOMINMAX
#include <windows.h>
#include <winbase.h>
#include <stdint.h>

#include "Dump.h"
#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"

#include "../SDK/ConVar.h"
#include "../SDK/Entity.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/Localize.h"
#include "../SDK/Surface.h"
#include "../SDK/Vector.h"
#include "../SDK/WeaponData.h"
#include "../SDK/GameEvent.h"

static constexpr bool worldToScreen(const Vector& in, Vector& out) noexcept
{
    const auto& matrix = interfaces->engine->worldToScreenMatrix();
    float w = matrix._41 * in.x + matrix._42 * in.y + matrix._43 * in.z + matrix._44;

    if (w > 0.001f) {
        const auto [width, height] = interfaces->surface->getScreenSize();
        out.x = width / 2 * (1 + (matrix._11 * in.x + matrix._12 * in.y + matrix._13 * in.z + matrix._14) / w);
        out.y = height / 2 * (1 - (matrix._21 * in.x + matrix._22 * in.y + matrix._23 * in.z + matrix._24) / w);
        out.z = 0.0f;
        return true;
    }
    return false;
}

struct BoundingBox {
    float x0, y0;
    float x1, y1;
    Vector vertices[8];

    BoundingBox(Entity* entity) noexcept
    {
        const auto [width, height] = interfaces->surface->getScreenSize();

        x0 = static_cast<float>(width * 2);
        y0 = static_cast<float>(height * 2);
        x1 = -x0;
        y1 = -y0;

        const auto& mins = entity->getCollideable()->obbMins();
        const auto& maxs = entity->getCollideable()->obbMaxs();

        for (int i = 0; i < 8; ++i) {
            const Vector point{ i & 1 ? maxs.x : mins.x,
                                i & 2 ? maxs.y : mins.y,
                                i & 4 ? maxs.z : mins.z };

            if (!worldToScreen(point.transform(entity->toWorldTransform()), vertices[i])) {
                valid = false;
                return;
            }
            x0 = std::min(x0, vertices[i].x);
            y0 = std::min(y0, vertices[i].y);
            x1 = std::max(x1, vertices[i].x);
            y1 = std::max(y1, vertices[i].y);
        }
        valid = true;
    }

    operator bool() noexcept
    {
        return valid;
    }
private:
    bool valid;
};

int64_t s_time = -1;
int64_t current_time = -1;

uint64_t GetSystemTimeAsUnixTime()
{
    //Get the number of seconds since January 1, 1970 12:00am UTC
    //Code released into public domain; no attribution required.

    const uint64_t UNIX_TIME_START = 0x019DB1DED53E8000; //January 1, 1970 (start of Unix epoch) in "ticks"
    const uint64_t TICKS_PER_SECOND = 10000; //a tick is 100ns

    FILETIME ft;
    GetSystemTimeAsFileTime(&ft); //returns ticks in UTC

    //Copy the low and high parts of FILETIME into a LARGE_INTEGER
    //This is so we can access the full 64-bits as an Int64 without causing an alignment fault
    LARGE_INTEGER li;
    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;

    //Convert ticks since 1/1/1970 into seconds
    return (li.QuadPart - UNIX_TIME_START) / TICKS_PER_SECOND;
}

void Dump::dump_player_death(GameEvent& event) noexcept {
    if (config->dump.enabled && interfaces->engine->isInGame())
    {
        FILE* fout = fopen(config->dump.FilePath.c_str(), "a+");
        LARGE_INTEGER start, f;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&start);
        uint64_t ms_interval = start.QuadPart / (f.QuadPart / 1000);
        
        fprintf(fout, "%lld,", current_time + ms_interval - s_time);

        fprintf(fout, "player_death, ");

        const auto player = interfaces->entityList->getEntity(interfaces->engine->getPlayerForUserID(event.getInt("userid")));
        fprintf(fout, "\"%s\", ", player->getPlayerName(true).c_str());

        fprintf(fout, "%d, ", event.getInt("headshot"));
        fprintf(fout, "%d, ", event.getInt("penetrated"));
        fprintf(fout, "\n");
        fclose(fout);
    }
}

void Dump::dump_event_userid(GameEvent& event, const char* event_name) noexcept {
    if (config->dump.enabled && interfaces->engine->isInGame())
    {
        FILE* fout = fopen(config->dump.FilePath.c_str(), "a+");
        LARGE_INTEGER start, f;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&start);
        uint64_t ms_interval = start.QuadPart / (f.QuadPart / 1000);

        fprintf(fout, "%lld, ", current_time + ms_interval - s_time);

        fprintf(fout, "%s, ", event_name);

        const auto player = interfaces->entityList->getEntity(interfaces->engine->getPlayerForUserID(event.getInt("userid")));
        fprintf(fout, "\"%s\", ", player->getPlayerName(true).c_str());

        fprintf(fout, "\n");
        fclose(fout);
    }
}

void Dump::dump() noexcept {
    if (config->dump.enabled && interfaces->engine->isInGame())
    {
        FILE* fout = fopen(config->dump.FilePath.c_str(), "a+");
        
        LARGE_INTEGER start, f;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&start);
        uint64_t ms_interval = start.QuadPart / (f.QuadPart / 1000);

        if (s_time == -1)
        {
            s_time = ms_interval;
            current_time = GetSystemTimeAsUnixTime();

        }
        if (ms_interval - s_time <= 2000)
        {
            fclose(fout);
            return;
        }
        if (!localPlayer && localPlayer->isAlive())
        {
            fclose(fout);
            return;
        }
      
        const auto observerTarget = localPlayer->getObserverTarget();

        const auto activeWeapon = localPlayer.get()->getActiveWeapon();
        
        if (!activeWeapon)
        {
            fclose(fout);
            return;
        }
        fprintf(fout, "%lld,", current_time + ms_interval - s_time); // timestamp
        fprintf(fout, "player_pos, "); // event
        fprintf(fout, "%d, ", localPlayer->health()); // player health
        fprintf(fout, "%s, ", activeWeapon->getWeaponData()->name); // player weapon
        
        Vector player_eye{ localPlayer->getEyePosition() };
        fprintf(fout, "%f, %f, %f, ", player_eye.x, player_eye.y, player_eye.z); // player pos

        for (int i = 1; i <= interfaces->engine->getMaxClients(); i++)
        {
            auto entity = interfaces->entityList->getEntity(i);
            if (!entity || entity == localPlayer.get() || entity == observerTarget || entity->isDormant() || !entity->isAlive())
            {
                continue;
            }

            if (PlayerInfo playerInfo; interfaces->engine->getPlayerInfo(entity->index(), playerInfo))
            {
                fprintf(fout, "\"%s\", ", playerInfo.name); // op_name
            }
            else
            {
                fprintf(fout, "\"\", ");
            }
            fprintf(fout, "%d, ", entity->health()); // op_health
            // Àû Ã¼·Â
            int boneList[] = {
                8, // head
                7, 6, 5, 4, 3, 0, // spine
                11, 12, 13, // left arm
                41, 42, 43, // right arm
                70, 71, 72, // left leg
                77, 78, 79  // right leg
            };
            for (int i = 0; i < sizeof(boneList) / sizeof(int); i++)
            {
                auto bonePosition = entity->getBonePosition(boneList[i]);
                Vector sbonePos;
                if (worldToScreen(bonePosition, sbonePos))
                {
                    fprintf(fout, "%f, %f, ", sbonePos.x, sbonePos.y);
                    fprintf(fout, "%d, ", entity->isVisible(bonePosition));
                }
                else
                {
                    fprintf(fout, ",,,");
                }
            }
        }
              
        fprintf(fout, "\n");
        fclose(fout);

    }
}