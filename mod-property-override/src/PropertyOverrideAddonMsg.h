#ifndef MOD_PROPERTY_OVERRIDE_ADDON_MSG_H
#define MOD_PROPERTY_OVERRIDE_ADDON_MSG_H

#include "Define.h"
#include <string>
#include <string_view>
#include <vector>

// Wire protocol between the PropertyOverlay client addon and the server.
// Pure string handling — no core dependencies — so it is unit-testable and
// safe to fuzz. Everything here parses UNTRUSTED client input.
//
// Client -> server (addon whisper to self, LANG_ADDON, swallowed server-side):
//   IPROP\tQ\tE\t<invSlot>          query equipped slot (client 1-based inv slot id)
//   IPROP\tQ\tB\t<bag>\t<slot>      query bag slot (client bag 0-4, slot 1-based)
// Server -> client:
//   IPROP\tR\tE\t<invSlot>\t<prop>:<value>:<expiry>;...   rows present
//   IPROP\tR\tE\t<invSlot>\t-                             no overrides
//   (same shape with B\t<bag>\t<slot> for bag queries)
//   IPROP\tI                                              invalidate: flush addon cache
//
// Coordinates are echoed back verbatim (client conventions); the server only
// converts them when resolving the item. Total chat payload is capped at 255
// bytes server-side, so replies truncate whole rows to fit.

namespace mod_property_override::addon
{

constexpr std::string_view PREFIX = "IPROP";
constexpr size_t MAX_PAYLOAD = 255;

struct Query
{
    enum class Kind : uint8
    {
        Invalid = 0,
        Equipped,
        Bag
    };

    Kind kind = Kind::Invalid;
    uint32 bag = 0;   // raw client value (bag queries only)
    uint32 slot = 0;  // raw client value
};

struct RowView
{
    uint8 property;
    int32 value;
    uint64 expiry;
};

// True if msg is addressed to this module ("IPROP\t...").
bool IsAddonMessage(std::string_view msg);

// Parses a full incoming message (including prefix). Returns Kind::Invalid
// on any malformed input; never throws.
Query ParseQuery(std::string_view msg);

// Builds the reply for a query, echoing its coordinates. Rows that would
// push the payload past MAX_PAYLOAD are dropped (whole rows only).
std::string BuildReply(Query const& query, std::vector<RowView> const& rows);

// Cache-flush push, sent after an override changes server-side.
std::string BuildInvalidate();

} // namespace mod_property_override::addon

#endif // MOD_PROPERTY_OVERRIDE_ADDON_MSG_H
