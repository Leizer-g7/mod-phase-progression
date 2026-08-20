#ifndef MOD_PHASE_PROGRESSION_PROFESSION_PHASE_ADAPTER_H
#define MOD_PHASE_PROGRESSION_PROFESSION_PHASE_ADAPTER_H

#include <cstdint>

class Player;

namespace ProfessionPhaseAdapter
{
    void ClampAll(Player* player);

    void HandleSetSkill(
        Player* player,
        std::uint32_t skillId,
        std::uint32_t newValue,
        std::uint32_t newMax);

    bool CanLearnTrainerSpell(
        Player const* player,
        std::uint32_t spellId);
}

#endif
