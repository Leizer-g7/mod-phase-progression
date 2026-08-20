#ifndef MOD_PHASE_PROGRESSION_BATTLEGROUND_PHASE_ADAPTER_H
#define MOD_PHASE_PROGRESSION_BATTLEGROUND_PHASE_ADAPTER_H

#include <cstdint>
#include <string>

struct PhaseDefinition;

struct BattlegroundRuntimeState
{
    bool joinBG = false;
    bool autoJoinBG = false;

    std::string wsBrackets;
    std::string abBrackets;
    std::string avBrackets;
    std::string eyBrackets;
    std::string icBrackets;

    std::uint32_t wsCount = 0;
    std::uint32_t abCount = 0;
    std::uint32_t avCount = 0;
    std::uint32_t eyCount = 0;
    std::uint32_t icCount = 0;
};

namespace BattlegroundPhaseAdapter
{
    bool Apply(
        PhaseDefinition const& phase,
        std::string& error);

    BattlegroundRuntimeState GetRuntimeState();
}

#endif
