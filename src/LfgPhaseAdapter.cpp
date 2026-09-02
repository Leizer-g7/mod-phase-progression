#include "LfgPhaseAdapter.h"

#include "PhaseMgr.h"

#include "Config.h"
#include "LFGMgr.h"
#include "Log.h"
#include "PlayerbotAIConfig.h"

namespace
{
    bool IsRandomDungeonFinderEnabled(uint32 options)
    {
        return
            (options &
             lfg::LFG_OPTION_ENABLE_DUNGEON_FINDER) != 0;
    }
}

namespace LfgPhaseAdapter
{
    bool Apply(
        PhaseDefinition const& phase,
        std::string& error)
    {
        (void)error;

        /*
         * Autoridad global RDF
         *
         * LFGMgr parte del DungeonFinder.OptionsMask configurado
         * por AzerothCore. Desde ese momento PhaseMgr modifica
         * únicamente el bit de Random Dungeon Finder sobre el
         * estado runtime actual y conserva intactos:
         *
         *   0x02 = Raid Browser
         *   0x04 = Seasonal Bosses
         */
        bool rdfEnabled =
            sConfigMgr->GetOption<bool>(
                "Progression.LFG.RandomDungeonFinder.Enabled",
                true);

        uint32 options =
            sLFGMgr->GetOptions();

        if (rdfEnabled)
        {
            options |=
                lfg::LFG_OPTION_ENABLE_DUNGEON_FINDER;
        }
        else
        {
            options &=
                ~static_cast<uint32>(
                    lfg::LFG_OPTION_ENABLE_DUNGEON_FINDER);
        }

        /*
         * LFGMgr mantiene una copia runtime propia de OptionsMask.
         * La autoridad RDF debe aplicarse directamente al manager.
         */
        sLFGMgr->SetOptions(options);

        /*
         * Automatización Playerbots.
         *
         * Progression.LFG.Enabled conserva su semántica original:
         * permitir que los randombots utilicen LFG.
         *
         * Un bot nunca debe intentar entrar al RDF si el RDF
         * global está cerrado.
         */
        bool randomBotEnabled =
            sConfigMgr->GetOption<bool>(
                "Progression.LFG.Enabled",
                true);

        sPlayerbotAIConfig.randomBotJoinLfg =
            randomBotEnabled && rdfEnabled;

        LOG_INFO(
            "module",
            "PhaseProgression: LFG aplicado. "
            "Phase={}, RDF={}, OptionsMask={}, RandomBotJoinLfg={}.",
            static_cast<unsigned>(phase.phase),
            rdfEnabled ? "ENABLED" : "DISABLED",
            options,
            sPlayerbotAIConfig.randomBotJoinLfg
                ? "ENABLED"
                : "DISABLED");

        return true;
    }

    LfgRuntimeState GetRuntimeState()
    {
        LfgRuntimeState state;

        state.options =
            sLFGMgr->GetOptions();

        state.randomDungeonFinderEnabled =
            IsRandomDungeonFinderEnabled(
                state.options);

        state.randomBotJoinLfg =
            sPlayerbotAIConfig.randomBotJoinLfg;

        return state;
    }
}
