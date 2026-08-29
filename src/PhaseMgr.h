#ifndef MOD_PHASE_PROGRESSION_PHASE_MGR_H
#define MOD_PHASE_PROGRESSION_PHASE_MGR_H

#include <cstdint>
#include <map>
#include <string>

struct PhaseDefinition
{
    std::uint8_t phase = 20;

    // Rango de contenido histórico permitido mientras esta
    // fase de nivel está activa.
    std::uint8_t contentStageMin = 0;
    std::uint8_t contentStageMax = 0;

    std::uint8_t maxPlayerLevel = 20;
    std::uint8_t randomBotMaxLevel = 20;

    std::uint32_t botGearMaxItemLevel = 25;

    // Límite económico usado exclusivamente por AHBot
    // para materias primas. No afecta al gear de Playerbots.
    std::uint32_t auctionMaterialMaxItemLevel = 20;

    std::string botDistribution;

    std::uint16_t craftingCap = 150;
    std::uint16_t gatheringCap = 150;
    std::uint16_t secondaryCap = 150;
    std::uint16_t ridingCap = 75;

    bool outlandEnabled = false;
    bool northrendEnabled = false;
    bool deathKnightEnabled = false;
};

class PhaseMgr
{
public:
    static PhaseMgr& Instance();

    void LoadConfig();
    void LoadState();

    bool SetActivePhase(std::uint8_t phase);
    bool SetActiveContentStage(std::uint8_t stage);

    bool IsValidPhase(std::uint8_t phase) const;
    bool IsValidContentStageForPhase(
        std::uint8_t phase,
        std::uint8_t stage) const;

    PhaseDefinition const& GetActiveDefinition() const;
    PhaseDefinition const& GetDefinition(std::uint8_t phase) const;

    std::uint8_t GetActivePhase() const;
    std::uint8_t GetActiveContentStage() const;
    std::uint8_t GetMaxPlayerLevel() const;
    std::uint8_t GetRandomBotMaxLevel() const;

    std::uint16_t GetProfessionCap(std::uint32_t skillId) const;

    bool IsEnabled() const;
    bool IsRollbackEnabled() const;
    bool IsGMBypassEnabled() const;

private:
    PhaseMgr() = default;

    void LoadPhase(
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
        std::string const& defaultDistribution);

    bool _enabled = true;
    bool _allowRollback = false;
    bool _gmBypass = true;

    std::uint8_t _defaultPhase = 20;
    std::uint8_t _defaultContentStage = 0;

    std::uint8_t _activePhase = 20;
    std::uint8_t _activeContentStage = 0;

    std::map<std::uint8_t, PhaseDefinition> _phases;
};

#define sPhaseMgr PhaseMgr::Instance()

#endif
