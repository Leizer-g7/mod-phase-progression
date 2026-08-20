#include "PlayerbotsPhaseAdapter.h"

#include "PhaseMgr.h"

#include "Config.h"
#include "Log.h"
#include "PlayerbotAIConfig.h"
#include "RandomBotLevelMgr.h"

#include <cctype>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    std::string Trim(std::string value)
    {
        while (!value.empty() &&
               std::isspace(static_cast<unsigned char>(value.front())))
        {
            value.erase(value.begin());
        }

        while (!value.empty() &&
               std::isspace(static_cast<unsigned char>(value.back())))
        {
            value.pop_back();
        }

        return value;
    }

    bool ParseUInt(
        std::string const& value,
        std::uint32_t& result)
    {
        try
        {
            std::size_t processed = 0;

            unsigned long parsed =
                std::stoul(value, &processed);

            if (processed != value.size())
                return false;

            result = static_cast<std::uint32_t>(parsed);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ParseDistribution(
        std::string const& text,
        std::uint8_t maxLevel,
        std::vector<LevelBracketConfig>& result,
        std::string& error)
    {
        result.clear();

        std::stringstream stream(text);
        std::string token;

        std::uint32_t percentageTotal = 0;
        std::uint32_t previousUpper = 0;

        while (std::getline(stream, token, ','))
        {
            token = Trim(token);

            if (token.empty())
                continue;

            std::size_t colon = token.find(':');

            if (colon == std::string::npos)
            {
                error =
                    "Bracket inválido '" + token +
                    "': falta ':'.";
                return false;
            }

            std::string rangeText =
                Trim(token.substr(0, colon));

            std::string pctText =
                Trim(token.substr(colon + 1));

            std::uint32_t lower = 0;
            std::uint32_t upper = 0;
            std::uint32_t pct = 0;

            std::size_t dash =
                rangeText.find('-');

            if (dash == std::string::npos)
            {
                if (!ParseUInt(rangeText, lower))
                {
                    error =
                        "Nivel inválido en bracket '" +
                        token + "'.";
                    return false;
                }

                upper = lower;
            }
            else
            {
                std::string lowerText =
                    Trim(rangeText.substr(0, dash));

                std::string upperText =
                    Trim(rangeText.substr(dash + 1));

                if (!ParseUInt(lowerText, lower) ||
                    !ParseUInt(upperText, upper))
                {
                    error =
                        "Rango inválido en bracket '" +
                        token + "'.";
                    return false;
                }
            }

            if (!ParseUInt(pctText, pct))
            {
                error =
                    "Porcentaje inválido en bracket '" +
                    token + "'.";
                return false;
            }

            if (lower < 1 ||
                upper < lower ||
                upper > maxLevel)
            {
                error =
                    "Rango fuera de límites en '" +
                    token + "'.";
                return false;
            }

            if (pct > 100)
            {
                error =
                    "Porcentaje mayor de 100 en '" +
                    token + "'.";
                return false;
            }

            /*
             * Queremos que los brackets cubran todos
             * los niveles sin huecos.
             *
             * Ejemplo:
             * 1-9,10-19,20
             */
            if (result.empty())
            {
                if (lower != 1)
                {
                    error =
                        "El primer bracket debe comenzar "
                        "en nivel 1.";
                    return false;
                }
            }
            else if (lower != previousUpper + 1)
            {
                error =
                    "Los brackets deben ser contiguos. "
                    "Error cerca de '" + token + "'.";
                return false;
            }

            LevelBracketConfig bracket;

            bracket.lower =
                static_cast<uint8>(lower);

            bracket.upper =
                static_cast<uint8>(upper);

            bracket.pct =
                static_cast<uint8>(pct);

            result.push_back(bracket);

            previousUpper = upper;
            percentageTotal += pct;
        }

        if (result.empty())
        {
            error = "La distribución de bots está vacía.";
            return false;
        }

        if (previousUpper != maxLevel)
        {
            error =
                "El último bracket debe terminar en el "
                "RandomBotMaxLevel de la fase.";
            return false;
        }

        if (percentageTotal != 100)
        {
            error =
                "Los porcentajes de BotDistribution "
                "deben sumar exactamente 100.";
            return false;
        }

        return true;
    }

}

namespace PlayerbotsPhaseAdapter
{
    bool Apply(
        PhaseDefinition const& phase,
        std::string& error)
    {
        std::vector<LevelBracketConfig> ranges;

        if (!ParseDistribution(
                phase.botDistribution,
                phase.randomBotMaxLevel,
                ranges,
                error))
        {
            LOG_ERROR(
                "module",
                "PhaseProgression: error aplicando "
                "Playerbots: {}",
                error);

            return false;
        }

        /*
         * CAP GLOBAL DE RND BOTS
         */
        sPlayerbotAIConfig.randomBotMaxLevel =
            phase.randomBotMaxLevel;

        /*
         * VIAJES A CAPITALES DE EXPANSIONES
         *
         * No quitamos los mapas 530/571 de RandomBotMaps,
         * porque map 530 también contiene las zonas iniciales
         * de Blood Elf y Draenei.
         *
         * En su lugar deshabilitamos los intentos automáticos
         * de teleport a Shattrath/Dalaran mientras la expansión
         * correspondiente permanezca cerrada.
         *
         * Al abrirla restauramos el valor configurado originalmente
         * en playerbots.conf, en vez de forzar siempre 1.
         */
        int const configuredShattrathWeight =
            sConfigMgr->GetOption<int>(
                "AiPlayerbot.TeleToShattrathCityWeight",
                1);

        int const configuredDalaranWeight =
            sConfigMgr->GetOption<int>(
                "AiPlayerbot.TeleToDalaranWeight",
                1);

        sPlayerbotAIConfig.weightTeleToShattrathCity =
            phase.outlandEnabled
                ? configuredShattrathWeight
                : 0;

        sPlayerbotAIConfig.weightTeleToDalaran =
            phase.northrendEnabled
                ? configuredDalaranWeight
                : 0;


        LOG_INFO(
            "module",
            "PhaseProgression: Playerbots travel. "
            "Phase={}, ShattrathWeight={}, DalaranWeight={}.",
            static_cast<unsigned>(phase.phase),
            sPlayerbotAIConfig.weightTeleToShattrathCity,
            sPlayerbotAIConfig.weightTeleToDalaran);

        /*
         * LEVEL BRACKETS
         */
        sPlayerbotAIConfig.levelBracketsEnabled = true;

        /*
         * PHASE PROGRESSION:
         * el cap de fase es global.
         *
         * Guilds con jugadores reales, arena teams y
         * friend lists NO pueden permitir que un random bot
         * permanezca por encima del cap activo.
         */
        sPlayerbotAIConfig.levelBracketsIgnoreGuildWithRealPlayers = false;
        sPlayerbotAIConfig.levelBracketsIgnoreArenaTeamBots = false;
        sPlayerbotAIConfig.levelBracketsIgnoreFriendListed = false;

        /*
         * Acelerar la convergencia después de un cambio
         * de fase sin procesar cientos de bots de golpe.
         */
        sPlayerbotAIConfig.levelBracketsCheckFrequency = 30;
        sPlayerbotAIConfig.levelBracketsFlaggedCheckFrequency = 10;
        sPlayerbotAIConfig.levelBracketsFlaggedProcessLimit = 10;

        sPlayerbotAIConfig.levelBracketsDynamicDistribution =
            false;

        sPlayerbotAIConfig.levelBracketsSyncFactions =
            false;

        sPlayerbotAIConfig.levelBracketsNumRanges =
            static_cast<uint8>(ranges.size());

        /*
         * Queremos la misma distribución
         * Alliance/Horde.
         */
        sPlayerbotAIConfig.levelBracketsAlliance =
            ranges;

        sPlayerbotAIConfig.levelBracketsHorde =
            ranges;

        /*
         * GEAR DE RND BOTS
         *
         * Los nombres dicen "Score", pero estos valores
         * representan límites de item level.
         */
        sPlayerbotAIConfig.randomGearScoreLimit =
            static_cast<int32>(
                phase.botGearMaxItemLevel);

        sPlayerbotAIConfig.autoGearScoreLimit =
            static_cast<int32>(
                phase.botGearMaxItemLevel);

        /*
         * DK PLAYERBOTS
         */
        sPlayerbotAIConfig.disableDeathKnightLogin =
            !phase.deathKnightEnabled;

        /*
         * SmartScale no debería trabajar por encima
         * del cap actual.
         */
        sPlayerbotAIConfig.botActiveAloneSmartScaleWhenMaxLevel =
            phase.randomBotMaxLevel;

        /*
         * RandomBotLevelMgr mantiene una copia runtime
         * independiente. Hay que volver a cargarla.
         */
        RandomBotLevelMgr::instance().LoadConfig();

        LOG_INFO(
            "module",
            "PhaseProgression: Playerbots aplicado. "
            "Phase={}, MaxLevel={}, Ranges={}, "
            "GearMaxILvl={}, DK={}.",
            static_cast<unsigned>(phase.phase),
            static_cast<unsigned>(
                phase.randomBotMaxLevel),
            ranges.size(),
            phase.botGearMaxItemLevel,
            phase.deathKnightEnabled
                ? "ENABLED"
                : "DISABLED");

        return true;
    }
    PlayerbotsRuntimeState GetRuntimeState()
    {
        PlayerbotsRuntimeState state;

        state.maxLevel =
            static_cast<std::uint8_t>(
                sPlayerbotAIConfig.randomBotMaxLevel);

        state.bracketRanges =
            static_cast<std::uint8_t>(
                sPlayerbotAIConfig.levelBracketsNumRanges);

        state.bracketsEnabled =
            sPlayerbotAIConfig.levelBracketsEnabled;

        state.randomGearMaxItemLevel =
            static_cast<std::uint32_t>(
                sPlayerbotAIConfig.randomGearScoreLimit);

        state.autoGearMaxItemLevel =
            static_cast<std::uint32_t>(
                sPlayerbotAIConfig.autoGearScoreLimit);

        state.deathKnightEnabled =
            !sPlayerbotAIConfig.disableDeathKnightLogin;

        state.ignoreGuildWithRealPlayers =
            sPlayerbotAIConfig.levelBracketsIgnoreGuildWithRealPlayers;

        state.ignoreArenaTeamBots =
            sPlayerbotAIConfig.levelBracketsIgnoreArenaTeamBots;

        state.ignoreFriendListed =
            sPlayerbotAIConfig.levelBracketsIgnoreFriendListed;

        return state;
    }

}
