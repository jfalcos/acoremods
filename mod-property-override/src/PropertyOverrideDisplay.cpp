#include "PropertyOverrideDisplay.h"

#include "Player.h"

#include <fmt/format.h>

namespace mod_property_override::display
{

char const* QualityColor(uint32 quality)
{
    switch (quality)
    {
        case 0: return "|cff333333";
        case 1: return "";           // default dark text
        case 2: return "|cff0f5c0f"; // uncommon, darkened
        case 3: return "|cff004a99";
        case 4: return "|cff5c1a99";
        case 5: return "|cff8a4a00"; // legendary, darkened
        default: return "";
    }
}

std::optional<LiveValue> LiveValueFor(Player* player, Property prop, uint32 delta)
{
    using P = Property;
    auto flat = [&](double now) -> std::optional<LiveValue>
    { return LiveValue{ now, static_cast<double>(delta), false }; };
    auto stat = [&](Stats s) { return flat(player->GetStat(s)); };
    auto rating = [&](CombatRating cr) -> std::optional<LiveValue>
    {
        uint32 cur = player->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + cr);
        float bonus = player->GetRatingBonusValue(cr);
        if (!cur || bonus <= 0.f)
            return std::nullopt;
        double d = bonus * static_cast<double>(delta) / static_cast<double>(cur);
        return LiveValue{ bonus, d, true };
    };

    switch (prop)
    {
        case P::Strength:  return stat(STAT_STRENGTH);
        case P::Agility:   return stat(STAT_AGILITY);
        case P::Stamina:   return stat(STAT_STAMINA);
        case P::Intellect: return stat(STAT_INTELLECT);
        case P::Spirit:    return stat(STAT_SPIRIT);
        case P::Health:            return flat(player->GetMaxHealth());
        case P::Mana:              return flat(player->GetMaxPower(POWER_MANA));
        case P::AttackPower:       return flat(player->GetTotalAttackPowerValue(BASE_ATTACK));
        case P::RangedAttackPower: return flat(player->GetTotalAttackPowerValue(RANGED_ATTACK));
        case P::SpellPower:        return flat(player->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SPELL));
        case P::Armor:             return flat(player->GetArmor());
        case P::HolyResistance:    return flat(player->GetResistance(SPELL_SCHOOL_HOLY));
        case P::FireResistance:    return flat(player->GetResistance(SPELL_SCHOOL_FIRE));
        case P::NatureResistance:  return flat(player->GetResistance(SPELL_SCHOOL_NATURE));
        case P::FrostResistance:   return flat(player->GetResistance(SPELL_SCHOOL_FROST));
        case P::ShadowResistance:  return flat(player->GetResistance(SPELL_SCHOOL_SHADOW));
        case P::ArcaneResistance:  return flat(player->GetResistance(SPELL_SCHOOL_ARCANE));
        case P::DefenseRating:     return rating(CR_DEFENSE_SKILL);
        case P::DodgeRating:       return rating(CR_DODGE);
        case P::ParryRating:       return rating(CR_PARRY);
        case P::BlockRating:       return rating(CR_BLOCK);
        case P::HitMeleeRating:    return rating(CR_HIT_MELEE);
        case P::HitRangedRating:   return rating(CR_HIT_RANGED);
        case P::HitSpellRating:    return rating(CR_HIT_SPELL);
        case P::CritMeleeRating:   return rating(CR_CRIT_MELEE);
        case P::CritRangedRating:  return rating(CR_CRIT_RANGED);
        case P::CritSpellRating:   return rating(CR_CRIT_SPELL);
        case P::HasteMeleeRating:  return rating(CR_HASTE_MELEE);
        case P::HasteRangedRating: return rating(CR_HASTE_RANGED);
        case P::HasteSpellRating:  return rating(CR_HASTE_SPELL);
        case P::ExpertiseRating:   return rating(CR_EXPERTISE);
        case P::ArmorPenetrationRating: return rating(CR_ARMOR_PENETRATION);
        default:
            return std::nullopt; // tri-ratings, regen, spell pen: no clean single value
    }
}

std::string FormatStatDisplay(Player* player, Property prop,
                              uint32 delta, int32 currentBonus)
{
    if (auto live = LiveValueFor(player, prop, delta))
    {
        if (live->percent)
            return fmt::format("{}{:.1f}%{} ({:.1f}%+{}{:.1f}%{})",
                               COL_GREEN, live->now + live->delta, COL_END,
                               live->now, COL_GREEN, live->delta, COL_END);
        return fmt::format("{}{}{} ({}+{}{}{})",
                           COL_GREEN, static_cast<int64>(live->now + live->delta), COL_END,
                           static_cast<int64>(live->now),
                           COL_GREEN, static_cast<int64>(live->delta), COL_END);
    }
    return fmt::format("{}+{}{} (+{}+{}{}{})",
                       COL_GREEN, currentBonus + static_cast<int32>(delta), COL_END,
                       currentBonus, COL_GREEN, delta, COL_END);
}

} // namespace mod_property_override::display
