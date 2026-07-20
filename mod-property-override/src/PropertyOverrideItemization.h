#ifndef MOD_PROPERTY_OVERRIDE_ITEMIZATION_H
#define MOD_PROPERTY_OVERRIDE_ITEMIZATION_H

#include "PropertyOverrideMgr.h"
#include <string_view>
#include <vector>

// Shared itemization facts — pure, no engine dependencies, unit-testable.
// Extracted from mod-paragon's item-upgrade math (2026-07-20) when
// mod-item-infusion became a second consumer: what a property point *costs*
// and what a native item's stat budget *is* are facts about the property
// vocabulary and the item corpus, not about any one shop.

namespace mod_property_override
{

// Approximate Blizzard itemization cost of one point of the property
// (primaries/ratings/SP 1.0, AP 0.5, HP5/MP5 2.5, health/mana 0.05,
// armor 0.1). Tri-rating conveniences apply three rating mods per point and
// weigh triple. Returns 0 for ids outside the vocabulary.
float PropertyWeight(Property p);

// Native full-item stat budget for (quality, itemLevel). Fitted
// through-origin slopes (weighted budget points per ItemLevel) from the
// acore_world equippable corpus, 18k+ items, 2026-07-19:
//   quality 2: 0.4111 (n=4243), 3: 0.5794 (n=4666), 4: 1.1999 (n=9315).
// Qualities without native stat data get conservative values.
float NativeBudget(uint32 quality, uint32 itemLevel);

// Weighted budget points consumed by the rows of one source namespace
// ('paragon' = coin upgrades, 'mix' = infusions, ...). Rows from other
// sources never count against a system's own budget.
float BudgetSpent(std::vector<OverrideRow> const& rows, std::string_view source);

} // namespace mod_property_override

#endif // MOD_PROPERTY_OVERRIDE_ITEMIZATION_H
