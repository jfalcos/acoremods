#ifndef MOD_PARAGON_PERKS_H
#define MOD_PARAGON_PERKS_H

#include "PropertyOverrideProperties.h"
#include <array>
#include <optional>
#include <string_view>

// Ranked stat perks — the AA spend side. Pure math, no engine dependencies,
// unit-testable. Bought ranks are applied as mod-property-override rows
// (source 'paragon'); paragon_perk_ranks is the rank source of truth.

namespace mod_paragon::perks
{

using mod_property_override::Property;

// The v1 buyable set: five primaries + the two power stats.
constexpr std::array<Property, 7> PERK_SET =
{
    Property::Strength,
    Property::Agility,
    Property::Stamina,
    Property::Intellect,
    Property::Spirit,
    Property::AttackPower,
    Property::SpellPower,
};

struct PerkConfig
{
    // valuePerRank indexed like PERK_SET.
    std::array<uint32, PERK_SET.size()> valuePerRank = { 5, 5, 5, 5, 5, 10, 6 };
    uint32 maxRanks = 20;
    uint32 costStepEvery = 5; // cost = 1 + ranks/costStepEvery coins
};

bool IsPerkProperty(Property p);
std::optional<size_t> PerkIndex(Property p);

// Coin cost of buying the NEXT rank when `currentRanks` are owned.
// Returns 0 if currentRanks >= maxRanks (not purchasable).
uint32 CostForNextRank(PerkConfig const& cfg, uint32 currentRanks);

// Total stat value granted by `ranks` of the perk at PERK_SET index `idx`.
uint32 TotalValue(PerkConfig const& cfg, size_t idx, uint32 ranks);

} // namespace mod_paragon::perks

#endif // MOD_PARAGON_PERKS_H
