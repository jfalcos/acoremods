#ifndef MOD_PROPERTY_OVERRIDE_DISPLAY_H
#define MOD_PROPERTY_OVERRIDE_DISPLAY_H

#include "PropertyOverrideProperties.h"
#include <optional>
#include <string>

class Player;

// Shared GOSSIP-surface display helpers for property values, extracted from
// mod-paragon's Quartermaster (2026-07-20) so every consumer NPC UI
// (Quartermaster, Alchemist, ...) renders stats identically — the
// sweep-all-surfaces rule made structural.
//
// SURFACE CONTRACT: these colors are tuned for the LIGHT parchment gossip
// frame (dark default text -> accents must be DARK). Chat-frame output uses
// the bright standard colors instead — do not reuse this palette there.

namespace mod_property_override::display
{

// Gossip color palette (user-validated on the parchment; see module README).
// "Now" values take the plain default dark text — color marks only what
// changes; dead rows near-black gray; costs deep violet.
constexpr char const* COL_GRAY  = "|cff333333"; // dead rows only
constexpr char const* COL_GREEN = "|cff0a4a0a"; // "after" values
constexpr char const* COL_COST  = "|cff4b0082"; // costs, budgets
constexpr char const* COL_END   = "|r";

// Item quality colors, darkened where the canonical shade is too light for
// parchment. Common quality inherits the default dark text.
char const* QualityColor(uint32 quality);

struct LiveValue
{
    double now;
    double delta;
    bool percent;
};

// Best-effort live character value + the delta a `delta`-point purchase of
// the property adds. Rating percents derive from the player's own current
// rating->bonus ratio, so no private APIs needed. std::nullopt when no
// clean single value exists (tri-ratings, regen, spell pen, zero ratings).
std::optional<LiveValue> LiveValueFor(Player* player, Property prop, uint32 delta);

// WoW-convention stat display: "Total (base+bonus)" with the modified total
// and the bonus in green, e.g. "Agility: 161 (156+5)". Falls back to the
// item-bonus form "+15 (+10+5)" when no live value exists.
std::string FormatStatDisplay(Player* player, Property prop,
                              uint32 delta, int32 currentBonus);

} // namespace mod_property_override::display

#endif // MOD_PROPERTY_OVERRIDE_DISPLAY_H
