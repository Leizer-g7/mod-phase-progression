#ifndef MOD_PHASE_PROGRESSION_LFG_PHASE_ADAPTER_H
#define MOD_PHASE_PROGRESSION_LFG_PHASE_ADAPTER_H

#include <cstdint>
#include <string>

struct PhaseDefinition;

struct LfgRuntimeState
{
    std::uint32_t options = 0;
    bool randomDungeonFinderEnabled = false;
    bool randomBotJoinLfg = false;
};

namespace LfgPhaseAdapter
{
    bool Apply(
        PhaseDefinition const& phase,
        std::string& error);

    LfgRuntimeState GetRuntimeState();
}

#endif
