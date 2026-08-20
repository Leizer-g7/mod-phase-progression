#include "ProfessionPhaseAdapter.h"

#include "PhaseMgr.h"

#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace
{
    thread_local bool gProfessionClampInProgress = false;

    class ProfessionClampGuard
    {
    public:
        ProfessionClampGuard()
        {
            gProfessionClampInProgress = true;
        }

        ~ProfessionClampGuard()
        {
            gProfessionClampInProgress = false;
        }
    };

    constexpr std::array<std::uint32_t, 15> ProfessionSkills =
    {
        129, // First Aid
        185, // Cooking
        356, // Fishing

        164, // Blacksmithing
        165, // Leatherworking
        171, // Alchemy
        197, // Tailoring
        202, // Engineering
        333, // Enchanting
        755, // Jewelcrafting
        773, // Inscription

        182, // Herbalism
        186, // Mining
        393, // Skinning

        762  // Riding
    };

    bool IsBypassed(Player const* player)
    {
        return !player ||
               !sPhaseMgr.IsEnabled() ||
               (sPhaseMgr.IsGMBypassEnabled() &&
                player->IsGameMaster());
    }

    std::uint16_t GetMaxAllowedStep(
        std::uint16_t cap)
    {
        if (!cap)
            return 0;

        /*
         * Ranks normales de skills:
         *
         *  75 -> step 1
         * 150 -> step 2
         * 225 -> step 3
         * 300 -> step 4
         * 375 -> step 5
         * 450 -> step 6
         */
        return std::max<std::uint16_t>(
            1,
            cap / 75);
    }

    void ClampSkill(
        Player* player,
        std::uint32_t skillId)
    {
        if (IsBypassed(player) ||
            gProfessionClampInProgress)
        {
            return;
        }

        std::uint16_t cap =
            sPhaseMgr.GetProfessionCap(skillId);

        if (!cap || !player->HasSkill(skillId))
            return;

        std::uint16_t currentValue =
            player->GetPureSkillValue(skillId);

        /*
         * OnPlayerGetMaxSkillValue ya expone como máximo
         * el cap de fase.
         */
        std::uint16_t exposedMax =
            player->GetPureMaxSkillValue(skillId);

        std::uint16_t targetMax =
            std::min<std::uint16_t>(
                exposedMax,
                cap);

        if (!targetMax)
            return;

        std::uint16_t targetValue =
            std::min<std::uint16_t>(
                currentValue,
                targetMax);

        std::uint16_t currentStep =
            player->GetSkillStep(
                static_cast<std::uint16_t>(skillId));

        std::uint16_t targetStep =
            std::min<std::uint16_t>(
                currentStep,
                GetMaxAllowedStep(cap));

        if (!targetStep)
            targetStep = 1;

        ProfessionClampGuard guard;

        player->SetSkill(
            static_cast<std::uint16_t>(skillId),
            targetStep,
            targetValue,
            targetMax);
    }

    bool SpellWouldExceedCap(
        Player const* player,
        std::uint32_t spellId,
        std::uint32_t depth)
    {
        if (!spellId || depth > 4)
            return false;

        /*
         * Algunos spells tienen información directa
         * SpellLearnSkill.
         */
        if (SpellLearnSkillNode const* learnSkill =
                sSpellMgr->GetSpellLearnSkill(spellId))
        {
            std::uint16_t cap =
                sPhaseMgr.GetProfessionCap(
                    learnSkill->skill);

            if (cap)
            {
                std::uint32_t newMax =
                    learnSkill->maxvalue
                        ? learnSkill->maxvalue
                        : player->GetMaxSkillValueForLevel();

                if (newMax > cap)
                    return true;

                if (learnSkill->step >
                    GetMaxAllowedStep(cap))
                {
                    return true;
                }
            }
        }

        SpellInfo const* spellInfo =
            sSpellMgr->GetSpellInfo(spellId);

        if (!spellInfo)
            return false;

        for (SpellEffectInfo const& effect :
             spellInfo->GetEffects())
        {
            /*
             * SPELL_EFFECT_SKILL_STEP termina en
             * EffectLearnSkill() -> SetSkill().
             */
            if (effect.IsEffect(
                    SPELL_EFFECT_SKILL_STEP))
            {
                std::uint32_t skillId =
                    effect.MiscValue;

                std::uint16_t cap =
                    sPhaseMgr.GetProfessionCap(
                        skillId);

                if (cap)
                {
                    std::int32_t step =
                        effect.CalcValue();

                    if (step > 0)
                    {
                        std::uint32_t newMax =
                            static_cast<std::uint32_t>(
                                step) * 75;

                        if (newMax > cap)
                            return true;

                        if (static_cast<std::uint32_t>(
                                step) >
                            GetMaxAllowedStep(cap))
                        {
                            return true;
                        }
                    }
                }
            }

            /*
             * Muchos trainers enseñan un wrapper que
             * a su vez contiene SPELL_EFFECT_LEARN_SPELL.
             * Seguimos el TriggerSpell para comprobar
             * también el spell realmente aprendido.
             */
            if (effect.IsEffect(
                    SPELL_EFFECT_LEARN_SPELL) &&
                effect.TriggerSpell)
            {
                if (SpellWouldExceedCap(
                        player,
                        effect.TriggerSpell,
                        depth + 1))
                {
                    return true;
                }
            }
        }

        return false;
    }
}

namespace ProfessionPhaseAdapter
{
    void ClampAll(Player* player)
    {
        if (IsBypassed(player))
            return;

        for (std::uint32_t skillId :
             ProfessionSkills)
        {
            ClampSkill(player, skillId);
        }
    }

    void HandleSetSkill(
        Player* player,
        std::uint32_t skillId,
        std::uint32_t newValue,
        std::uint32_t newMax)
    {
        if (IsBypassed(player) ||
            gProfessionClampInProgress)
        {
            return;
        }

        std::uint16_t cap =
            sPhaseMgr.GetProfessionCap(skillId);

        if (!cap)
            return;

        std::uint16_t currentStep =
            player->GetSkillStep(
                static_cast<std::uint16_t>(
                    skillId));

        if (newValue > cap ||
            newMax > cap ||
            currentStep > GetMaxAllowedStep(cap))
        {
            ClampSkill(player, skillId);
        }
    }

    bool CanLearnTrainerSpell(
        Player const* player,
        std::uint32_t spellId)
    {
        if (IsBypassed(player))
            return true;

        return !SpellWouldExceedCap(
            player,
            spellId,
            0);
    }
}
