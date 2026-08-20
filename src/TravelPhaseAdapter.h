#ifndef MOD_PHASE_PROGRESSION_TRAVEL_PHASE_ADAPTER_H
#define MOD_PHASE_PROGRESSION_TRAVEL_PHASE_ADAPTER_H

#include <cstdint>

class Player;

namespace TravelPhaseAdapter
{
    bool CanTeleport(
        Player* player,
        std::uint32_t mapId,
        float x,
        float y,
        float z);

    bool EnsureValidLocation(Player* player);
}

#endif
