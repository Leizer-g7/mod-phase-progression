#include "PhaseMgr.h"

#include "Config.h"
#include "DatabaseEnv.h"
#include "QueryResult.h"
#include "Log.h"

PhaseMgr& PhaseMgr::Instance()
{
    static PhaseMgr instance;
    return instance;
}

void PhaseMgr::LoadPhase(
    std::uint8_t phase,
    std::uint8_t defaultLevel,
    std::uint32_t defaultGear,
    std::uint16_t defaultCrafting,
    std::uint16_t defaultGathering,
    std::uint16_t defaultSecondary,
    std::uint16_t defaultRiding,
    bool defaultOutland,
    bool defaultNorthrend,
    bool defaultDK,
    std::string const& defaultDistribution)
{
    std::string prefix =
        "Progression.Phase" + std::to_string(static_cast<unsigned>(phase)) + ".";

    PhaseDefinition def;

    def.phase = phase;

    std::uint8_t defaultContentMin = 0;
    std::uint8_t defaultContentMax = 0;

    switch (phase)
    {
        case 60:
            defaultContentMin = 0;
            defaultContentMax = 8;
            break;

        case 70:
            defaultContentMin = 8;
            defaultContentMax = 13;
            break;

        case 80:
            defaultContentMin = 13;
            defaultContentMax = 18;
            break;

        default:
            defaultContentMin = 0;
            defaultContentMax = 0;
            break;
    }

    def.contentStageMin = static_cast<std::uint8_t>(
        sConfigMgr->GetOption<std::uint32_t>(
            prefix + "ContentStage.Min",
            defaultContentMin));

    def.contentStageMax = static_cast<std::uint8_t>(
        sConfigMgr->GetOption<std::uint32_t>(
            prefix + "ContentStage.Max",
            defaultContentMax));

    if (def.contentStageMin > def.contentStageMax ||
        def.contentStageMax > 18)
    {
        LOG_ERROR(
            "module",
            "PhaseProgression: rango ContentStage inválido "
            "para Phase={}. Configurado {}..{}, usando {}..{}.",
            static_cast<unsigned>(phase),
            static_cast<unsigned>(def.contentStageMin),
            static_cast<unsigned>(def.contentStageMax),
            static_cast<unsigned>(defaultContentMin),
            static_cast<unsigned>(defaultContentMax));

        def.contentStageMin = defaultContentMin;
        def.contentStageMax = defaultContentMax;
    }

    def.maxPlayerLevel = static_cast<std::uint8_t>(
        sConfigMgr->GetOption<std::uint32_t>(
            prefix + "MaxPlayerLevel",
            defaultLevel));

    def.randomBotMaxLevel = static_cast<std::uint8_t>(
        sConfigMgr->GetOption<std::uint32_t>(
            prefix + "RandomBotMaxLevel",
            defaultLevel));

    def.botDistribution =
        sConfigMgr->GetOption<std::string>(
            prefix + "BotDistribution",
            defaultDistribution);

    def.botGearMaxItemLevel =
        sConfigMgr->GetOption<std::uint32_t>(
            prefix + "BotGearMaxItemLevel",
            defaultGear);

    /*
     * Límite exclusivo para materias primas publicadas
     * por AuctionHouseBot.
     *
     * El default es el nivel de la fase, pero cada fase
     * puede configurarlo independientemente.
     */
    def.auctionMaterialMaxItemLevel =
        sConfigMgr->GetOption<std::uint32_t>(
            prefix + "AuctionMaterialMaxItemLevel",
            defaultLevel);


    def.craftingCap = static_cast<std::uint16_t>(
        sConfigMgr->GetOption<std::uint32_t>(
            prefix + "Profession.Crafting",
            defaultCrafting));

    def.gatheringCap = static_cast<std::uint16_t>(
        sConfigMgr->GetOption<std::uint32_t>(
            prefix + "Profession.Gathering",
            defaultGathering));

    def.secondaryCap = static_cast<std::uint16_t>(
        sConfigMgr->GetOption<std::uint32_t>(
            prefix + "Profession.Secondary",
            defaultSecondary));

    def.ridingCap = static_cast<std::uint16_t>(
        sConfigMgr->GetOption<std::uint32_t>(
            prefix + "Profession.Riding",
            defaultRiding));

    def.outlandEnabled =
        sConfigMgr->GetOption<bool>(
            prefix + "Outland",
            defaultOutland);

    def.northrendEnabled =
        sConfigMgr->GetOption<bool>(
            prefix + "Northrend",
            defaultNorthrend);

    def.deathKnightEnabled =
        sConfigMgr->GetOption<bool>(
            prefix + "DeathKnight",
            defaultDK);

    _phases[phase] = def;
}

void PhaseMgr::LoadConfig()
{
    _enabled =
        sConfigMgr->GetOption<bool>(
            "Progression.Enable",
            true);

    _allowRollback =
        sConfigMgr->GetOption<bool>(
            "Progression.AllowPhaseRollback",
            false);

    _gmBypass =
        sConfigMgr->GetOption<bool>(
            "Progression.GMBypass",
            true);

    _defaultPhase = static_cast<std::uint8_t>(
        sConfigMgr->GetOption<std::uint32_t>(
            "Progression.ActivePhase",
            20));

    _defaultContentStage = static_cast<std::uint8_t>(
        sConfigMgr->GetOption<std::uint32_t>(
            "Progression.ActiveContentStage",
            0));

    _phases.clear();

    LoadPhase(20, 20, 25, 150, 150, 150, 75, false, false, false,
        "1-9:15,10-19:50,20:35");

    LoadPhase(30, 30, 35, 225, 225, 225, 75, false, false, false,
        "1-9:10,10-19:15,20-29:45,30:30");

    LoadPhase(40, 40, 45, 300, 300, 300, 150, false, false, false,
        "1-9:8,10-19:10,20-29:15,30-39:42,40:25");

    LoadPhase(50, 50, 55, 300, 300, 300, 150, false, false, false,
        "1-9:6,10-19:8,20-29:10,30-39:12,40-49:40,50:24");

    LoadPhase(60, 60, 78, 300, 300, 300, 150, false, false, false,
        "1-9:5,10-19:7,20-29:8,30-39:10,40-49:12,50-59:38,60:20");

    LoadPhase(70, 70, 125, 375, 375, 375, 300, true, false, false,
        "1-9:4,10-19:5,20-29:6,30-39:7,40-49:8,50-59:10,60-69:40,70:20");

    LoadPhase(80, 80, 224, 450, 450, 450, 300, true, true, true,
        "1-9:3,10-19:4,20-29:5,30-39:6,40-49:7,50-59:8,60-69:10,70-79:37,80:20");

    if (!IsValidPhase(_defaultPhase))
    {
        LOG_ERROR(
            "module",
            "PhaseProgression: Progression.ActivePhase={} no es válido. Se usará 20.",
            static_cast<unsigned>(_defaultPhase));

        _defaultPhase = 20;
    }

    if (!IsValidContentStageForPhase(
            _defaultPhase,
            _defaultContentStage))
    {
        PhaseDefinition const& def =
            GetDefinition(_defaultPhase);

        LOG_ERROR(
            "module",
            "PhaseProgression: Progression.ActiveContentStage={} "
            "no es válido para Phase={}. Se usará {}.",
            static_cast<unsigned>(_defaultContentStage),
            static_cast<unsigned>(_defaultPhase),
            static_cast<unsigned>(def.contentStageMin));

        _defaultContentStage = def.contentStageMin;
    }

    if (!IsValidPhase(_activePhase))
        _activePhase = _defaultPhase;

    if (!IsValidContentStageForPhase(
            _activePhase,
            _activeContentStage))
    {
        _activeContentStage =
            GetDefinition(_activePhase).contentStageMin;
    }

    LOG_INFO(
        "module",
        "PhaseProgression: configuración cargada. "
        "Fase por defecto={}, ContentStage por defecto={}.",
        static_cast<unsigned>(_defaultPhase),
        static_cast<unsigned>(_defaultContentStage));
}

void PhaseMgr::LoadState()
{
    QueryResult result =
        WorldDatabase.Query(
            "SELECT `active_phase`, `active_content_stage` "
            "FROM `phase_progression_state` "
            "WHERE `id` = 1");

    if (!result)
    {
        _activePhase = _defaultPhase;
        _activeContentStage = _defaultContentStage;

        WorldDatabase.Execute(
            "INSERT INTO `phase_progression_state` "
            "(`id`, `active_phase`, `active_content_stage`) "
            "VALUES (1, {}, {}) "
            "ON DUPLICATE KEY UPDATE "
            "`active_phase` = VALUES(`active_phase`), "
            "`active_content_stage` = VALUES(`active_content_stage`)",
            static_cast<std::uint32_t>(_activePhase),
            static_cast<std::uint32_t>(_activeContentStage));

        LOG_INFO(
            "module",
            "PhaseProgression: sin estado persistido. "
            "Usando Phase={}, ContentStage={}.",
            static_cast<unsigned>(_activePhase),
            static_cast<unsigned>(_activeContentStage));

        return;
    }

    Field* fields = result->Fetch();

    std::uint8_t dbPhase =
        fields[0].Get<std::uint8_t>();

    std::uint8_t dbContentStage =
        fields[1].Get<std::uint8_t>();

    if (!IsValidPhase(dbPhase))
    {
        LOG_ERROR(
            "module",
            "PhaseProgression: fase {} inválida en DB. Usando {}.",
            static_cast<unsigned>(dbPhase),
            static_cast<unsigned>(_defaultPhase));

        _activePhase = _defaultPhase;
        return;
    }

    _activePhase = dbPhase;

    if (!IsValidContentStageForPhase(
            dbPhase,
            dbContentStage))
    {
        _activeContentStage =
            GetDefinition(dbPhase).contentStageMin;

        LOG_ERROR(
            "module",
            "PhaseProgression: ContentStage={} inválido "
            "para Phase={}. Se usará {}.",
            static_cast<unsigned>(dbContentStage),
            static_cast<unsigned>(dbPhase),
            static_cast<unsigned>(_activeContentStage));

        WorldDatabase.Execute(
            "UPDATE `phase_progression_state` "
            "SET `active_content_stage` = {} "
            "WHERE `id` = 1",
            static_cast<std::uint32_t>(_activeContentStage));
    }
    else
    {
        _activeContentStage = dbContentStage;
    }

    LOG_INFO(
        "module",
        "PhaseProgression: estado cargado desde DB. "
        "Phase={}, ContentStage={}.",
        static_cast<unsigned>(_activePhase),
        static_cast<unsigned>(_activeContentStage));
}

bool PhaseMgr::SetActivePhase(std::uint8_t phase)
{
    if (!IsValidPhase(phase))
        return false;

    if (!IsValidContentStageForPhase(
            phase,
            _activeContentStage))
    {
        return false;
    }

    if (!_allowRollback && phase < _activePhase)
        return false;

    _activePhase = phase;

    WorldDatabase.Execute(
        "INSERT INTO `phase_progression_state` "
        "(`id`, `active_phase`) VALUES (1, {}) "
        "ON DUPLICATE KEY UPDATE `active_phase` = VALUES(`active_phase`)",
        static_cast<std::uint32_t>(_activePhase));

    LOG_INFO(
        "module",
        "PhaseProgression: nueva fase activa: {}.",
        static_cast<unsigned>(_activePhase));

    return true;
}

bool PhaseMgr::SetActiveContentStage(std::uint8_t stage)
{
    if (!IsValidContentStageForPhase(
            _activePhase,
            stage))
    {
        return false;
    }

    if (!_allowRollback && stage < _activeContentStage)
        return false;

    _activeContentStage = stage;

    WorldDatabase.Execute(
        "INSERT INTO `phase_progression_state` "
        "(`id`, `active_phase`, `active_content_stage`) "
        "VALUES (1, {}, {}) "
        "ON DUPLICATE KEY UPDATE "
        "`active_content_stage` = VALUES(`active_content_stage`)",
        static_cast<std::uint32_t>(_activePhase),
        static_cast<std::uint32_t>(_activeContentStage));

    LOG_INFO(
        "module",
        "PhaseProgression: nuevo ContentStage activo: {}.",
        static_cast<unsigned>(_activeContentStage));

    return true;
}

bool PhaseMgr::IsValidPhase(std::uint8_t phase) const
{
    return _phases.find(phase) != _phases.end();
}

bool PhaseMgr::IsValidContentStageForPhase(
    std::uint8_t phase,
    std::uint8_t stage) const
{
    if (!IsValidPhase(phase))
        return false;

    PhaseDefinition const& def =
        GetDefinition(phase);

    return stage >= def.contentStageMin &&
           stage <= def.contentStageMax;
}

PhaseDefinition const& PhaseMgr::GetActiveDefinition() const
{
    return _phases.at(_activePhase);
}

PhaseDefinition const& PhaseMgr::GetDefinition(
    std::uint8_t phase) const
{
    return _phases.at(phase);
}

std::uint8_t PhaseMgr::GetActivePhase() const
{
    return _activePhase;
}

std::uint8_t PhaseMgr::GetActiveContentStage() const
{
    return _activeContentStage;
}

std::uint8_t PhaseMgr::GetMaxPlayerLevel() const
{
    return GetActiveDefinition().maxPlayerLevel;
}

std::uint8_t PhaseMgr::GetRandomBotMaxLevel() const
{
    return GetActiveDefinition().randomBotMaxLevel;
}

bool PhaseMgr::IsEnabled() const
{
    return _enabled;
}

bool PhaseMgr::IsRollbackEnabled() const
{
    return _allowRollback;
}

bool PhaseMgr::IsGMBypassEnabled() const
{
    return _gmBypass;
}

std::uint16_t PhaseMgr::GetProfessionCap(std::uint32_t skillId) const
{
    PhaseDefinition const& def = GetActiveDefinition();

    switch (skillId)
    {
        // Crafting
        case 164: // Blacksmithing
        case 165: // Leatherworking
        case 171: // Alchemy
        case 197: // Tailoring
        case 202: // Engineering
        case 333: // Enchanting
        case 755: // Jewelcrafting
        case 773: // Inscription
            return def.craftingCap;

        // Gathering
        case 182: // Herbalism
        case 186: // Mining
        case 393: // Skinning
            return def.gatheringCap;

        // Secondary
        case 129: // First Aid
        case 185: // Cooking
        case 356: // Fishing
            return def.secondaryCap;

        // Riding
        case 762:
            return def.ridingCap;

        default:
            return 0;
    }
}
