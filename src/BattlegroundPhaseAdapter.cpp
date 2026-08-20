#include "BattlegroundPhaseAdapter.h"

#include "PhaseMgr.h"

#include "Config.h"
#include "Log.h"
#include "PlayerbotAIConfig.h"

#include <cstdint>
#include <string>

namespace
{
    struct BattlegroundPlan
    {
        std::string ws = "0";
        std::string ab = "0";
        std::string av = "0";
        std::string ey = "0";
        std::string ic = "0";

        bool wsEnabled = false;
        bool abEnabled = false;
        bool avEnabled = false;
        bool eyEnabled = false;
        bool icEnabled = false;
    };

    bool BuildPlan(
        std::uint8_t phase,
        BattlegroundPlan& plan)
    {
        switch (phase)
        {
            case 20:
                /*
                 * WSG:
                 * 0 = 10-19
                 * 1 = 20-29
                 *
                 * AB:
                 * 0 = 20-29
                 */
                plan.ws = "0,1";
                plan.ab = "0";

                plan.wsEnabled = true;
                plan.abEnabled = true;
                return true;

            case 30:
                /*
                 * WSG:
                 * 1 = 20-29
                 * 2 = 30-39
                 *
                 * AB:
                 * 0 = 20-29
                 * 1 = 30-39
                 */
                plan.ws = "1,2";
                plan.ab = "0,1";

                plan.wsEnabled = true;
                plan.abEnabled = true;
                return true;

            case 40:
                plan.ws = "2,3";
                plan.ab = "1,2";

                plan.wsEnabled = true;
                plan.abEnabled = true;
                return true;

            case 50:
                plan.ws = "3,4";
                plan.ab = "2,3";

                plan.wsEnabled = true;
                plan.abEnabled = true;
                return true;

            case 60:
                /*
                 * AV comienza en su bracket:
                 * 0 = 51-60
                 */
                plan.ws = "4,5";
                plan.ab = "3,4";
                plan.av = "0";

                plan.wsEnabled = true;
                plan.abEnabled = true;
                plan.avEnabled = true;
                return true;

            case 70:
                /*
                 * AV:
                 * 0 = 51-60
                 * 1 = 61-70
                 *
                 * EotS:
                 * 0 = 61-69
                 * 1 = 70-79
                 */
                plan.ws = "5,6";
                plan.ab = "4,5";
                plan.av = "0,1";
                plan.ey = "0,1";

                plan.wsEnabled = true;
                plan.abEnabled = true;
                plan.avEnabled = true;
                plan.eyEnabled = true;
                return true;

            case 80:
                /*
                 * WSG:
                 * 6 = 70-79
                 * 7 = 80
                 *
                 * AB:
                 * 5 = 70-79
                 * 6 = 80
                 *
                 * AV:
                 * 2 = 71-79
                 * 3 = 80
                 *
                 * EotS:
                 * 1 = 70-79
                 * 2 = 80
                 *
                 * IoC:
                 * 0 = 71-79
                 * 1 = 80
                 */
                plan.ws = "6,7";
                plan.ab = "5,6";
                plan.av = "2,3";
                plan.ey = "1,2";
                plan.ic = "0,1";

                plan.wsEnabled = true;
                plan.abEnabled = true;
                plan.avEnabled = true;
                plan.eyEnabled = true;
                plan.icEnabled = true;
                return true;

            default:
                return false;
        }
    }
}

namespace BattlegroundPhaseAdapter
{
    bool Apply(
        PhaseDefinition const& phase,
        std::string& error)
    {
        bool enabled =
            sConfigMgr->GetOption<bool>(
                "Progression.Battlegrounds.Enabled",
                true);

        std::string policy =
            sConfigMgr->GetOption<std::string>(
                "Progression.Battlegrounds.Policy",
                "CURRENT_AND_PREVIOUS");

        int32 instances =
            sConfigMgr->GetOption<int32>(
                "Progression.Battlegrounds.InstancesPerBracket",
                1);

        /*
         * Para nuestra primera versión mantenemos
         * una sola política explícita.
         */
        if (policy != "CURRENT_AND_PREVIOUS")
        {
            error =
                "Progression.Battlegrounds.Policy inválido: "
                + policy;

            return false;
        }

        /*
         * El diseño del servidor exige como máximo
         * una instancia automática por bracket.
         */
        if (instances != 1)
        {
            error =
                "Progression.Battlegrounds."
                "InstancesPerBracket debe ser 1.";

            return false;
        }

        if (!enabled)
        {
            sPlayerbotAIConfig.randomBotAutoJoinBG = false;

            sPlayerbotAIConfig.randomBotAutoJoinBGWSCount = 0;
            sPlayerbotAIConfig.randomBotAutoJoinBGABCount = 0;
            sPlayerbotAIConfig.randomBotAutoJoinBGAVCount = 0;
            sPlayerbotAIConfig.randomBotAutoJoinBGEYCount = 0;
            sPlayerbotAIConfig.randomBotAutoJoinBGICCount = 0;

            LOG_INFO(
                "module",
                "PhaseProgression: Battleground automation "
                "disabled.");

            return true;
        }

        BattlegroundPlan plan;

        if (!BuildPlan(phase.phase, plan))
        {
            error =
                "No existe plan de Battlegrounds para fase "
                + std::to_string(
                    static_cast<unsigned>(phase.phase));

            return false;
        }

        /*
         * Habilitar participación y creación automática
         * de BGs por randombots.
         */
        sPlayerbotAIConfig.randomBotJoinBG = true;
        sPlayerbotAIConfig.randomBotAutoJoinBG = true;

        /*
         * Brackets por Battleground.
         */
        sPlayerbotAIConfig.randomBotAutoJoinWSBrackets =
            plan.ws;

        sPlayerbotAIConfig.randomBotAutoJoinABBrackets =
            plan.ab;

        sPlayerbotAIConfig.randomBotAutoJoinAVBrackets =
            plan.av;

        sPlayerbotAIConfig.randomBotAutoJoinEYBrackets =
            plan.ey;

        sPlayerbotAIConfig.randomBotAutoJoinICBrackets =
            plan.ic;

        /*
         * Count = cantidad máxima de batallas automáticas
         * por CADA bracket listado.
         */
        std::uint32_t count =
            static_cast<std::uint32_t>(instances);

        sPlayerbotAIConfig.randomBotAutoJoinBGWSCount =
            plan.wsEnabled ? count : 0;

        sPlayerbotAIConfig.randomBotAutoJoinBGABCount =
            plan.abEnabled ? count : 0;

        sPlayerbotAIConfig.randomBotAutoJoinBGAVCount =
            plan.avEnabled ? count : 0;

        sPlayerbotAIConfig.randomBotAutoJoinBGEYCount =
            plan.eyEnabled ? count : 0;

        sPlayerbotAIConfig.randomBotAutoJoinBGICCount =
            plan.icEnabled ? count : 0;

        /*
         * Este módulo controla BGs, no arenas.
         * Evitamos que AutoJoinBG pueda iniciar arenas
         * automáticamente por otra configuración.
         */
        sPlayerbotAIConfig.randomBotAutoJoinBGRatedArena2v2Count = 0;
        sPlayerbotAIConfig.randomBotAutoJoinBGRatedArena3v3Count = 0;
        sPlayerbotAIConfig.randomBotAutoJoinBGRatedArena5v5Count = 0;

        LOG_INFO(
            "module",
            "PhaseProgression: Battlegrounds aplicado. "
            "Phase={}, "
            "WS={}({}), "
            "AB={}({}), "
            "AV={}({}), "
            "EY={}({}), "
            "IC={}({}).",
            static_cast<unsigned>(phase.phase),
            plan.ws,
            sPlayerbotAIConfig.randomBotAutoJoinBGWSCount,
            plan.ab,
            sPlayerbotAIConfig.randomBotAutoJoinBGABCount,
            plan.av,
            sPlayerbotAIConfig.randomBotAutoJoinBGAVCount,
            plan.ey,
            sPlayerbotAIConfig.randomBotAutoJoinBGEYCount,
            plan.ic,
            sPlayerbotAIConfig.randomBotAutoJoinBGICCount);

        return true;
    }

    BattlegroundRuntimeState GetRuntimeState()
    {
        BattlegroundRuntimeState state;

        state.joinBG =
            sPlayerbotAIConfig.randomBotJoinBG;

        state.autoJoinBG =
            sPlayerbotAIConfig.randomBotAutoJoinBG;

        state.wsBrackets =
            sPlayerbotAIConfig.randomBotAutoJoinWSBrackets;

        state.abBrackets =
            sPlayerbotAIConfig.randomBotAutoJoinABBrackets;

        state.avBrackets =
            sPlayerbotAIConfig.randomBotAutoJoinAVBrackets;

        state.eyBrackets =
            sPlayerbotAIConfig.randomBotAutoJoinEYBrackets;

        state.icBrackets =
            sPlayerbotAIConfig.randomBotAutoJoinICBrackets;

        state.wsCount =
            sPlayerbotAIConfig.randomBotAutoJoinBGWSCount;

        state.abCount =
            sPlayerbotAIConfig.randomBotAutoJoinBGABCount;

        state.avCount =
            sPlayerbotAIConfig.randomBotAutoJoinBGAVCount;

        state.eyCount =
            sPlayerbotAIConfig.randomBotAutoJoinBGEYCount;

        state.icCount =
            sPlayerbotAIConfig.randomBotAutoJoinBGICCount;

        return state;
    }
}
