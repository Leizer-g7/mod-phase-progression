#ifndef MOD_PHASE_PROGRESSION_LFG_PHASE_ADAPTER_H
#define MOD_PHASE_PROGRESSION_LFG_PHASE_ADAPTER_H

#include <string>

struct PhaseDefinition;

namespace LfgPhaseAdapter
{
    bool Apply(
        PhaseDefinition const& phase,
        std::string& error);
}

#endif
