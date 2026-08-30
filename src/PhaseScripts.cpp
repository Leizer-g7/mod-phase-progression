#include "PhaseMgr.h"
#include "PlayerbotsPhaseAdapter.h"
#include "BattlegroundPhaseAdapter.h"
#include "LfgPhaseAdapter.h"
#include "ProfessionPhaseAdapter.h"
#include "TravelPhaseAdapter.h"

#include "Chat.h"
#include "Config.h"
#include "Log.h"
#include "MapMgr.h"
#include "Player.h"
#include "Pet.h"
#include "QuestDef.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "World.h"

#include <algorithm>

namespace
{
    bool ApplyProgressionRuntime(
        PhaseDefinition const& phase,
        std::string& error)
    {
        if (!PlayerbotsPhaseAdapter::Apply(
                phase,
                error))
        {
            return false;
        }

        if (!BattlegroundPhaseAdapter::Apply(
                phase,
                error))
        {
            return false;
        }

        if (!LfgPhaseAdapter::Apply(
                phase,
                error))
        {
            return false;
        }

        return true;
    }
}


using namespace Acore::ChatCommands;

namespace
{
    bool IsPhaseBypassed(Player* player)
    {
        return player &&
               sPhaseMgr.IsGMBypassEnabled() &&
               player->IsGameMaster();
    }
}

class PhaseProgressionWorldScript : public WorldScript
{
public:
    PhaseProgressionWorldScript()
        : WorldScript("PhaseProgressionWorldScript")
    {
    }

    void OnAfterConfigLoad(bool reload) override
    {
        sPhaseMgr.LoadConfig();

        if (reload)
        {
            sPhaseMgr.LoadState();

            /*
             * Otros módulos, especialmente Playerbots, también
             * procesan OnAfterConfigLoad() y pueden volver a cargar
             * valores estáticos desde playerbots.conf.
             *
             * No reaplicamos aquí para no depender del orden de
             * ejecución de los WorldScripts. Lo hacemos en el
             * siguiente world tick, cuando el ciclo de reload ya
             * terminó completamente.
             */
            _reapplyRuntimeAfterConfigReload = true;
        }
    }

    void OnUpdate(uint32 diff) override
    {
        /*
         * Una recarga global de configuración tiene prioridad.
         * La reaplicamos inmediatamente en el primer world tick
         * posterior al ciclo completo de OnAfterConfigLoad().
         */
        if (_reapplyRuntimeAfterConfigReload)
        {
            _reapplyRuntimeAfterConfigReload = false;

            if (!sPhaseMgr.IsEnabled())
                return;

            std::string error;

            if (!ApplyProgressionRuntime(
                    sPhaseMgr.GetActiveDefinition(),
                    error))
            {
                LOG_ERROR(
                    "module",
                    "PhaseProgression: no se pudo reaplicar "
                    "runtime después de config reload: {}",
                    error);

                return;
            }

            LOG_INFO(
                "module",
                "PhaseProgression: runtime reaplicado después "
                "de config reload. Phase={}.",
                static_cast<unsigned>(
                    sPhaseMgr.GetActivePhase()));

            return;
        }

        /*
         * Algunos comandos administrativos de mod-playerbots
         * llaman directamente a sPlayerbotAIConfig.Initialize()
         * y no generan OnAfterConfigLoad().
         *
         * Revisamos una vez por segundo unas pocas señales
         * controladas por progression. Si Playerbots volvió a
         * sus valores estáticos, reaplicamos toda la fase.
         */
        _runtimeWatchdogTimer += diff;

        if (_runtimeWatchdogTimer < 1000)
            return;

        _runtimeWatchdogTimer = 0;

        if (!sPhaseMgr.IsEnabled())
            return;

        PhaseDefinition const& phase =
            sPhaseMgr.GetActiveDefinition();

        PlayerbotsRuntimeState state =
            PlayerbotsPhaseAdapter::GetRuntimeState();

        bool driftDetected =
            state.maxLevel != phase.randomBotMaxLevel ||
            !state.bracketsEnabled ||
            state.randomGearMaxItemLevel !=
                phase.botGearMaxItemLevel ||
            state.autoGearMaxItemLevel !=
                phase.botGearMaxItemLevel ||
            state.deathKnightEnabled !=
                phase.deathKnightEnabled ||
            state.ignoreGuildWithRealPlayers ||
            state.ignoreArenaTeamBots ||
            state.ignoreFriendListed;

        if (!driftDetected)
            return;

        LOG_WARN(
            "module",
            "PhaseProgression: runtime drift detectado en "
            "Playerbots. Reaplicando Phase={}.",
            static_cast<unsigned>(
                sPhaseMgr.GetActivePhase()));

        std::string error;

        if (!ApplyProgressionRuntime(
                phase,
                error))
        {
            LOG_ERROR(
                "module",
                "PhaseProgression: no se pudo restaurar "
                "runtime después de detectar drift: {}",
                error);

            return;
        }

        LOG_INFO(
            "module",
            "PhaseProgression: runtime restaurado por "
            "watchdog. Phase={}.",
            static_cast<unsigned>(
                sPhaseMgr.GetActivePhase()));
    }

    void OnStartup() override
    {
        sPhaseMgr.LoadState();

        if (!sPhaseMgr.IsEnabled())
            return;

        std::string error;

        if (!ApplyProgressionRuntime(
                sPhaseMgr.GetActiveDefinition(),
                error))
        {
            LOG_ERROR(
                "module",
                "PhaseProgression: no se pudo aplicar "
                "Playerbots durante startup: {}",
                error);
        }
    }

private:
    bool _reapplyRuntimeAfterConfigReload = false;
    uint32 _runtimeWatchdogTimer = 0;
};

class PhaseProgressionPlayerScript : public PlayerScript
{
public:
    PhaseProgressionPlayerScript()
        : PlayerScript("PhaseProgressionPlayerScript")
    {
    }

    void OnPlayerSetMaxLevel(Player* player, uint32& maxPlayerLevel) override
    {
        if (!sPhaseMgr.IsEnabled() || IsPhaseBypassed(player))
            return;

        maxPlayerLevel =
            std::min<uint32>(
                maxPlayerLevel,
                sPhaseMgr.GetMaxPlayerLevel());
    }

    bool OnPlayerCanGiveLevel(Player* player, uint8 newLevel) override
    {
        if (!sPhaseMgr.IsEnabled() || IsPhaseBypassed(player))
            return true;

        return newLevel <= sPhaseMgr.GetMaxPlayerLevel();
    }

    void OnPlayerGiveXP(
        Player* player,
        uint32& amount,
        Unit* /*victim*/,
        uint8 xpSource) override
    {
        if (!sPhaseMgr.IsEnabled() ||
            !player ||
            IsPhaseBypassed(player))
        {
            return;
        }

        uint8 const maxPlayerLevel =
            sPhaseMgr.GetMaxPlayerLevel();

        if (!amount ||
            player->GetLevel() < maxPlayerLevel)
        {
            return;
        }

        /*
         * KillRewarder passes the same XP value to the player's pet
         * after OnPlayerGiveXP(). Because the global phase cap sets
         * player XP to zero here, preserve the pet reward first.
         *
         * This mirrors AzerothCore's normal rule:
         *   solo  -> 100%
         *   group -> 50%
         */
        if (xpSource == PlayerXPSource::XPSOURCE_KILL)
        {
            if (Pet* pet = player->GetPet())
            {
                pet->GivePetXP(
                    player->GetGroup()
                        ? amount / 2
                        : amount);
            }
        }

        amount = 0;
    }

    bool OnPlayerShouldBeRewardedWithMoneyInsteadOfExp(
        Player* player) override
    {
        if (!sPhaseMgr.IsEnabled() ||
            !player ||
            IsPhaseBypassed(player))
        {
            return false;
        }

        return player->GetLevel() >=
            sPhaseMgr.GetMaxPlayerLevel();
    }

    void OnPlayerQuestComputeMaxLevelMoney(
        Player* player,
        Quest const* quest,
        uint32& moneyValue) override
    {
        if (!sPhaseMgr.IsEnabled() ||
            !player ||
            !quest ||
            IsPhaseBypassed(player) ||
            player->GetLevel() < sPhaseMgr.GetMaxPlayerLevel())
        {
            return;
        }

        if (quest->HasFlag(QUEST_FLAGS_NO_MONEY_FROM_XP))
        {
            moneyValue = 0;
            return;
        }

        uint32 const xpValue =
            quest->XPValue(
                sPhaseMgr.GetMaxPlayerLevel());

        moneyValue = static_cast<uint32>(
            (xpValue * (6 * COPPER)) *
            sWorld->getRate(RATE_REWARD_BONUS_MONEY));
    }

    void OnPlayerGetMaxSkillValue(
        Player* player,
        uint32 skill,
        int32& result,
        bool /*IsPure*/) override
    {
        if (!sPhaseMgr.IsEnabled() || IsPhaseBypassed(player))
            return;

        uint16 cap =
            sPhaseMgr.GetProfessionCap(skill);

        if (cap && result > cap)
            result = cap;
    }

    bool OnPlayerCanUpdateSkill(
        Player* player,
        uint32 skillId) override
    {
        if (!sPhaseMgr.IsEnabled() ||
            !player ||
            IsPhaseBypassed(player))
        {
            return true;
        }

        uint16 cap =
            sPhaseMgr.GetProfessionCap(skillId);

        if (!cap)
            return true;

        return player->GetSkillValue(skillId) < cap;
    }

    void OnPlayerLogin(Player* player) override
    {
        ProfessionPhaseAdapter::ClampAll(player);
        TravelPhaseAdapter::EnsureValidLocation(player);
    }

    void OnPlayerSetSkill(
        Player* player,
        uint32 skillId,
        uint32 value,
        uint32 max,
        uint32 step,
        uint32 newValue) override
    {
        (void)value;
        (void)step;

        ProfessionPhaseAdapter::HandleSetSkill(
            player,
            skillId,
            newValue,
            max);
    }

    bool OnPlayerCanLearnTrainerSpell(
        Player const* player,
        uint32 spellId) override
    {
        return ProfessionPhaseAdapter::CanLearnTrainerSpell(
            player,
            spellId);
    }

    bool OnPlayerBeforeTeleport(
        Player* player,
        uint32 mapId,
        float x,
        float y,
        float z,
        float orientation,
        uint32 options,
        Unit* target) override
    {
        (void)orientation;
        (void)options;
        (void)target;

        return TravelPhaseAdapter::CanTeleport(
            player,
            mapId,
            x,
            y,
            z);
    }
};

class PhaseProgressionAccountScript : public AccountScript
{
public:
    PhaseProgressionAccountScript()
        : AccountScript("PhaseProgressionAccountScript")
    {
    }

    bool CanAccountCreateCharacter(
        uint32 /*accountId*/,
        uint8 /*charRace*/,
        uint8 charClass) override
    {
        if (!sPhaseMgr.IsEnabled())
            return true;

        if (charClass != CLASS_DEATH_KNIGHT)
            return true;

        return sPhaseMgr.GetActiveDefinition().deathKnightEnabled;
    }
};

class PhaseProgressionCommandScript : public CommandScript
{
public:
    PhaseProgressionCommandScript()
        : CommandScript("PhaseProgressionCommandScript")
    {
    }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable progressionTable =
        {
            { "status",  HandleStatusCommand,  SEC_GAMEMASTER, Console::Yes },
            { "phase",   HandlePhaseCommand,   SEC_ADMINISTRATOR, Console::Yes },
            { "content", HandleContentCommand, SEC_ADMINISTRATOR, Console::Yes },
            { "reload",  HandleReloadCommand,  SEC_ADMINISTRATOR, Console::Yes },
        };

        static ChatCommandTable commandTable =
        {
            { "progression", progressionTable },
        };

        return commandTable;
    }

    static bool HandleStatusCommand(ChatHandler* handler)
    {
        PhaseDefinition const& def =
            sPhaseMgr.GetActiveDefinition();

        handler->SendSysMessage(
            "========== SERVER PROGRESSION ==========");

        handler->PSendSysMessage(
            "Active Phase: {}", static_cast<uint32>(def.phase));

        handler->PSendSysMessage(
            "Content Stage: {} (allowed {}..{})",
            static_cast<uint32>(
                sPhaseMgr.GetActiveContentStage()),
            static_cast<uint32>(def.contentStageMin),
            static_cast<uint32>(def.contentStageMax));

        handler->PSendSysMessage(
            "Rollback: {}",
            sPhaseMgr.IsRollbackEnabled()
                ? "ENABLED"
                : "DISABLED");

        handler->PSendSysMessage(
            "Player Max Level: {}", static_cast<uint32>(def.maxPlayerLevel));

        handler->PSendSysMessage(
            "RandomBot Max Level: {}", static_cast<uint32>(def.randomBotMaxLevel));

        handler->PSendSysMessage(
            "Bot Distribution: {}", def.botDistribution);

        handler->PSendSysMessage(
            "Bot Gear Max iLvl: {}", def.botGearMaxItemLevel);

        PlayerbotsRuntimeState runtime =
            PlayerbotsPhaseAdapter::GetRuntimeState();

        handler->SendSysMessage(
            "---------- PLAYERBOTS RUNTIME ----------");

        handler->PSendSysMessage(
            "Runtime Max Level: {}",
            static_cast<uint32>(runtime.maxLevel));

        handler->PSendSysMessage(
            "Runtime Brackets Enabled: {}",
            runtime.bracketsEnabled ? "YES" : "NO");

        handler->PSendSysMessage(
            "Runtime Bracket Ranges: {}",
            static_cast<uint32>(runtime.bracketRanges));

        handler->PSendSysMessage(
            "Runtime RandomGear Max iLvl: {}",
            runtime.randomGearMaxItemLevel);

        handler->PSendSysMessage(
            "Runtime AutoGear Max iLvl: {}",
            runtime.autoGearMaxItemLevel);

        handler->PSendSysMessage(
            "Runtime Death Knights: {}",
            runtime.deathKnightEnabled
                ? "OPEN"
                : "LOCKED");

        handler->PSendSysMessage(
            "Guild Bypass: {}",
            runtime.ignoreGuildWithRealPlayers
                ? "ON"
                : "OFF");

        handler->PSendSysMessage(
            "Arena Bypass: {}",
            runtime.ignoreArenaTeamBots
                ? "ON"
                : "OFF");

        handler->PSendSysMessage(
            "Friend Bypass: {}",
            runtime.ignoreFriendListed
                ? "ON"
                : "OFF");

        BattlegroundRuntimeState bgRuntime =
            BattlegroundPhaseAdapter::GetRuntimeState();

        handler->SendSysMessage(
            "---------- BATTLEGROUNDS RUNTIME ----------");

        handler->PSendSysMessage(
            "Join BG: {}",
            bgRuntime.joinBG ? "YES" : "NO");

        handler->PSendSysMessage(
            "Auto Join BG: {}",
            bgRuntime.autoJoinBG ? "YES" : "NO");

        handler->PSendSysMessage(
            "WSG Brackets: {} | Instances/Bracket: {}",
            bgRuntime.wsBrackets,
            bgRuntime.wsCount);

        handler->PSendSysMessage(
            "AB Brackets: {} | Instances/Bracket: {}",
            bgRuntime.abBrackets,
            bgRuntime.abCount);

        handler->PSendSysMessage(
            "AV Brackets: {} | Instances/Bracket: {}",
            bgRuntime.avBrackets,
            bgRuntime.avCount);

        handler->PSendSysMessage(
            "EotS Brackets: {} | Instances/Bracket: {}",
            bgRuntime.eyBrackets,
            bgRuntime.eyCount);

        handler->PSendSysMessage(
            "IoC Brackets: {} | Instances/Bracket: {}",
            bgRuntime.icBrackets,
            bgRuntime.icCount);

        handler->SendSysMessage(
            "----------------------------------------");

        handler->PSendSysMessage(
            "Crafting: {}", def.craftingCap);

        handler->PSendSysMessage(
            "Gathering: {}", def.gatheringCap);

        handler->PSendSysMessage(
            "Secondary: {}", def.secondaryCap);

        handler->PSendSysMessage(
            "Riding: {}", def.ridingCap);

        handler->PSendSysMessage(
            "Outland: {}",
            def.outlandEnabled ? "OPEN" : "LOCKED");

        handler->PSendSysMessage(
            "Northrend: {}",
            def.northrendEnabled ? "OPEN" : "LOCKED");

        handler->PSendSysMessage(
            "Death Knights: {}",
            def.deathKnightEnabled ? "OPEN" : "LOCKED");

        handler->SendSysMessage(
            "========================================");

        return true;
    }

    static bool HandlePhaseCommand(
        ChatHandler* handler,
        uint32 requestedPhase)
    {
        if (requestedPhase > 255)
        {
            handler->SendSysMessage("Invalid phase.");
            return true;
        }

        uint8 newPhase =
            static_cast<uint8>(requestedPhase);

        if (!sPhaseMgr.IsValidPhase(newPhase))
        {
            handler->SendSysMessage(
                "Valid phases: 20, 30, 40, 50, 60, 70, 80.");

            return true;
        }

        uint8 currentContentStage =
            sPhaseMgr.GetActiveContentStage();

        if (!sPhaseMgr.IsValidContentStageForPhase(
                newPhase,
                currentContentStage))
        {
            PhaseDefinition const& requestedDefinition =
                sPhaseMgr.GetDefinition(newPhase);

            handler->PSendSysMessage(
                "Phase {} does not accept current ContentStage {}. "
                "Allowed range is {}..{}.",
                static_cast<uint32>(newPhase),
                static_cast<uint32>(currentContentStage),
                static_cast<uint32>(
                    requestedDefinition.contentStageMin),
                static_cast<uint32>(
                    requestedDefinition.contentStageMax));

            return true;
        }

        uint8 oldPhase =
            sPhaseMgr.GetActivePhase();

        if (newPhase < oldPhase &&
            !sPhaseMgr.IsRollbackEnabled())
        {
            handler->PSendSysMessage(
                "Phase rollback is disabled. Current={}, requested={}.",
                static_cast<uint32>(oldPhase),
                static_cast<uint32>(newPhase));

            return true;
        }

        if (newPhase == oldPhase)
        {
            handler->PSendSysMessage(
                "Phase {} is already active.",
                static_cast<uint32>(newPhase));

            return true;
        }

        PhaseDefinition const& oldDefinition =
            sPhaseMgr.GetActiveDefinition();

        PhaseDefinition const& newDefinition =
            sPhaseMgr.GetDefinition(newPhase);

        std::string runtimeError;

        /*
         * Aplicamos primero el runtime de la nueva fase.
         * La fase NO se persiste hasta saber que Playerbots,
         * Battlegrounds y LFG pudieron aplicarse.
         */
        if (!ApplyProgressionRuntime(
                newDefinition,
                runtimeError))
        {
            std::string restoreError;

            ApplyProgressionRuntime(
                oldDefinition,
                restoreError);

            handler->PSendSysMessage(
                "Phase NOT changed. Runtime validation "
                "failed for phase {}: {}",
                static_cast<uint32>(newPhase),
                runtimeError);

            return true;
        }

        /*
         * Solo después de aplicar correctamente el runtime
         * cambiamos y persistimos la fase global.
         */
        if (!sPhaseMgr.SetActivePhase(newPhase))
        {
            std::string restoreError;

            ApplyProgressionRuntime(
                oldDefinition,
                restoreError);

            handler->SendSysMessage(
                "Unable to persist progression phase. "
                "Previous runtime restored.");

            return true;
        }

        handler->PSendSysMessage(
            "Progression phase changed: {} -> {}.",
            static_cast<uint32>(oldPhase),
            static_cast<uint32>(newPhase));

        return true;
    }

    static bool HandleContentCommand(
        ChatHandler* handler,
        uint32 requestedStage)
    {
        if (requestedStage > 18)
        {
            handler->SendSysMessage(
                "Invalid ContentStage. Valid global range: 0..18.");

            return true;
        }

        uint8 newStage =
            static_cast<uint8>(requestedStage);

        uint8 oldStage =
            sPhaseMgr.GetActiveContentStage();

        PhaseDefinition const& def =
            sPhaseMgr.GetActiveDefinition();

        if (!sPhaseMgr.IsValidContentStageForPhase(
                def.phase,
                newStage))
        {
            handler->PSendSysMessage(
                "ContentStage {} is invalid for Phase {}. "
                "Allowed range is {}..{}.",
                static_cast<uint32>(newStage),
                static_cast<uint32>(def.phase),
                static_cast<uint32>(def.contentStageMin),
                static_cast<uint32>(def.contentStageMax));

            return true;
        }

        if (newStage < oldStage &&
            !sPhaseMgr.IsRollbackEnabled())
        {
            handler->PSendSysMessage(
                "Content rollback is disabled. Current={}, requested={}.",
                static_cast<uint32>(oldStage),
                static_cast<uint32>(newStage));

            return true;
        }

        if (newStage == oldStage)
        {
            handler->PSendSysMessage(
                "ContentStage {} is already active.",
                static_cast<uint32>(newStage));

            return true;
        }

        if (!sPhaseMgr.SetActiveContentStage(newStage))
        {
            handler->SendSysMessage(
                "Unable to persist ContentStage.");

            return true;
        }

        handler->PSendSysMessage(
            "ContentStage changed: {} -> {}.",
            static_cast<uint32>(oldStage),
            static_cast<uint32>(newStage));

        return true;
    }

    static bool HandleReloadCommand(ChatHandler* handler)
    {
        LOG_INFO(
            "module",
            "PhaseProgression: administrative configuration "
            "reload requested.");

        /*
         * Recarga real de worldserver.conf + configuraciones
         * de módulos.
         *
         * Esto dispara OnAfterConfigLoad(true), donde
         * PhaseProgression vuelve a ejecutar LoadConfig()
         * y LoadState().
         *
         * El runtime de Playerbots/BG/LFG se reaplica en el
         * siguiente world tick para evitar depender del orden
         * de los WorldScripts durante el reload.
         */
        sWorld->LoadConfigSettings(true);

        /*
         * Igual que el comando oficial .reload config:
         * refrescar también las distancias de visibilidad
         * dependientes de configuración.
         */
        sMapMgr->InitializeVisibilityDistanceInfo();

        handler->PSendSysMessage(
            "Configuration reloaded. "
            "Active phase={}, ContentStage={}, Rollback={}.",
            static_cast<uint32>(
                sPhaseMgr.GetActivePhase()),
            static_cast<uint32>(
                sPhaseMgr.GetActiveContentStage()),
            sPhaseMgr.IsRollbackEnabled()
                ? "ENABLED"
                : "DISABLED");

        handler->SendSysMessage(
            "Progression runtime will be reapplied "
            "on the next world tick.");

        return true;
    }

};

void AddSC_phase_progression_scripts()
{
    new PhaseProgressionWorldScript();
    new PhaseProgressionPlayerScript();
    new PhaseProgressionAccountScript();
    new PhaseProgressionCommandScript();
}
