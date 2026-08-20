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

    if (!IsValidPhase(_activePhase))
        _activePhase = _defaultPhase;

    LOG_INFO(
        "module",
        "PhaseProgression: configuración cargada. Fase por defecto={}.",
        static_cast<unsigned>(_defaultPhase));
}

void PhaseMgr::LoadState()
{
    QueryResult result =
        WorldDatabase.Query(
            "SELECT `active_phase` "
            "FROM `phase_progression_state` "
            "WHERE `id` = 1");

    if (!result)
    {
        _activePhase = _defaultPhase;

        WorldDatabase.Execute(
            "INSERT INTO `phase_progression_state` "
            "(`id`, `active_phase`) VALUES (1, {}) "
            "ON DUPLICATE KEY UPDATE `active_phase` = VALUES(`active_phase`)",
            static_cast<std::uint32_t>(_activePhase));

        LOG_INFO(
            "module",
            "PhaseProgression: sin estado persistido. Usando fase {}.",
            static_cast<unsigned>(_activePhase));

        return;
    }

    Field* fields = result->Fetch();

    std::uint8_t dbPhase =
        fields[0].Get<std::uint8_t>();

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

    LOG_INFO(
        "module",
        "PhaseProgression: fase activa cargada desde DB: {}.",
        static_cast<unsigned>(_activePhase));
}

bool PhaseMgr::SetActivePhase(std::uint8_t phase)
{
    if (!IsValidPhase(phase))
        return false;

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

bool PhaseMgr::IsValidPhase(std::uint8_t phase) const
{
    return _phases.find(phase) != _phases.end();
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
