#ifndef MOD_PARAGON_ITEM_UPGRADES_H
#define MOD_PARAGON_ITEM_UPGRADES_H

#include "PropertyOverrideMgr.h"
#include <array>
#include <string_view>
#include <vector>

// Item-upgrade math — pure, no engine dependencies, unit-testable.
//
// Design: the mod-property-override row on an item instance IS the upgrade
// state. Purchases come in fixed per-property chunks; an item's total upgrade
// room is a budget derived from its (Quality, ItemLevel) using slopes fitted
// against the real item corpus (18k+ equippables in acore_world, 2026-07-19):
//   quality 2: 0.4111 weighted pts/ilvl, 3: 0.5794, 4: 1.1999.
// Weights approximate Blizzard itemization costs (primaries/ratings/SP 1.0,
// AP 0.5, HP5/MP5 2.5, health/mana 0.05).

namespace mod_paragon::upgrades
{

using mod_property_override::OverrideRow;
using mod_property_override::Property;

struct UpgradeConfig
{
    // Fraction of the item's native stat budget available for upgrades.
    float budgetPercent = 0.30f;
};

struct PropertyDef
{
    Property prop;
    uint32 chunk;       // stat granted per purchase
    float weight;       // budget points per stat point (itemization cost)
    char const* label;  // player-facing name ("Crit Rating (all schools)")
};

// Menu categories (gossip menus can't hold 40 rows).
enum class Category : uint8
{
    Primary = 0,
    OffenseRatings,
    DefenseRatings,
    PowerAndRegen,
    Defenses,
    Max
};

char const* CategoryName(Category c);

// Full table covering every property in mod_property_override::AllProperties().
std::vector<PropertyDef> const& AllUpgradeDefs();
PropertyDef const* FindDef(Property p);
Category CategoryOf(Property p);
std::vector<PropertyDef> const& DefsInCategory(Category c);

// Native full-item stat budget for (quality, itemLevel); upgrade cap is
// budgetPercent of this.
float NativeBudget(uint32 quality, uint32 itemLevel);
float UpgradeBudget(UpgradeConfig const& cfg, uint32 quality, uint32 itemLevel);

// Weighted budget points already consumed by the item's override rows.
float BudgetSpent(std::vector<OverrideRow> const& rows);

// Coin cost of the next chunk given how full the budget is (0.0-1.0):
// 1 coin below 1/3, up to 4 approaching the cap.
uint32 CostForNextChunk(float spentFraction);

} // namespace mod_paragon::upgrades

#endif // MOD_PARAGON_ITEM_UPGRADES_H
