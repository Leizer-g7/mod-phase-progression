#ifndef MOD_PHASE_PROGRESSION_PLAYERBOTS_PHASE_ADAPTER_H
#define MOD_PHASE_PROGRESSION_PLAYERBOTS_PHASE_ADAPTER_H

#include <cstdint>
#include <string>

struct PhaseDefinition;

struct PlayerbotsRuntimeState
{
    std::uint8_t maxLevel = 0;

    std::uint8_t bracketRanges = 0;
    bool bracketsEnabled = false;

    std::uint32_t randomGearMaxItemLevel = 0;
    std::uint32_t autoGearMaxItemLevel = 0;

    bool deathKnightEnabled = false;

    bool ignoreGuildWithRealPlayers = false;
    bool ignoreArenaTeamBots = false;
    bool ignoreFriendListed = false;
};

namespace PlayerbotsPhaseAdapter
{
    bool Apply(PhaseDefinition const& phase, std::string& error);

    PlayerbotsRuntimeState GetRuntimeState();
}

#endif
