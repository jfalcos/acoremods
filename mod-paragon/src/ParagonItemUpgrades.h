#ifndef MOD_PARAGON_ITEM_UPGRADES_H
#define MOD_PARAGON_ITEM_UPGRADES_H

#include "PropertyOverrideItemization.h"
#include "PropertyOverrideMgr.h"
#include <array>
#include <string_view>
#include <vector>

// Item-upgrade SHOP math — pure, no engine dependencies, unit-testable.
//
// Design: the mod-property-override row on an item instance IS the upgrade
// state (source='paragon'). Purchases come in fixed per-property chunks; an
// item's total upgrade room is `budgetPercent` of its native (Quality,
// ItemLevel) budget. The itemization facts (per-property weights, the
// corpus-fitted native budget curve, per-source budget accounting) live in
// the platform: mod-property-override/src/PropertyOverrideItemization.h.
// This unit owns only what the paragon shop decides: chunk sizes, menu
// categories/labels, the coin cost curve, and the budget-percent cap.

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
    uint32 chunk;       // stat granted per purchase (sized so one purchase
                        // ≈ 5 weighted budget points across the board)
    char const* label;  // player-facing name ("Crit Rating (ALL, 3x budget)")
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

// Upgrade cap: budgetPercent of the platform's NativeBudget.
float UpgradeBudget(UpgradeConfig const& cfg, uint32 quality, uint32 itemLevel);

// Coin cost of the next chunk given how full the budget is (0.0-1.0):
// 1 coin below 1/3, up to 4 approaching the cap.
uint32 CostForNextChunk(float spentFraction);

} // namespace mod_paragon::upgrades

#endif // MOD_PARAGON_ITEM_UPGRADES_H
