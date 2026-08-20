#include "LfgPhaseAdapter.h"

#include "PhaseMgr.h"

#include "Config.h"
#include "Log.h"
#include "PlayerbotAIConfig.h"

namespace LfgPhaseAdapter
{
    bool Apply(
        PhaseDefinition const& phase,
        std::string& error)
    {
        (void)error;

        bool enabled =
            sConfigMgr->GetOption<bool>(
                "Progression.LFG.Enabled",
                true);

        /*
         * No mantenemos listas manuales de mazmorras.
         *
         * AzerothCore y Playerbots ya comprueban:
         * - MinLevel
         * - MaxLevel
         * - locks/requisitos LFG
         *
         * mod-phase-progression únicamente controla
         * que los randombots puedan utilizar LFG.
         */
        sPlayerbotAIConfig.randomBotJoinLfg =
            enabled;

        LOG_INFO(
            "module",
            "PhaseProgression: LFG aplicado. "
            "Phase={}, RandomBotJoinLfg={}.",
            static_cast<unsigned>(phase.phase),
            enabled ? "ENABLED" : "DISABLED");

        return true;
    }
}
