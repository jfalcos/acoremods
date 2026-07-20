#ifndef MOD_ITEM_INFUSION_ADDON_MSG_H
#define MOD_ITEM_INFUSION_ADDON_MSG_H

#include "Define.h"
#include <string>
#include <string_view>
#include <vector>

// Wire protocol between the InfusionForge client addon and the server.
// Pure string handling — no core dependencies — so it is unit-testable and
// safe to fuzz. Everything here parses UNTRUSTED client input. Follows the
// conventions of mod-property-override's IPROP protocol (255-byte payload
// cap, whole-entry chunking, client-side coordinates echoed verbatim).
//
// Client -> server (addon whisper to self, LANG_ADDON, swallowed):
//   IFUSE\tP\t<tinv>\t<dbag>\t<dslot>\t<coins>\t<submask>   preview
//   IFUSE\tX\t<tinv>\t<dbag>\t<dslot>\t<coins>\t<submask>   execute
//   (tinv = equipped inv slot 1-19; dbag 0-4, dslot 1-based;
//    submask = bitmask over the server's configured substances list)
// Server -> client:
//   IFUSE\tR\tH\t<riskPct>\t<basePct>\t<penaltyPct>\t<mixPts>\t<nativePts>
//   IFUSE\tR\tY\t<prop>:<amount>;...       yield lines (chunked)
//   IFUSE\tR\tS\t<idx>:<itemId>:<redPct>:<eligible>;...   substances
//   IFUSE\tR\tE\t<code>                    refusal (LEVEL, NOYIELD, BASIC,
//                                          NOITEM, OFF)
//   IFUSE\tX\tOK | IFUSE\tX\tDEAD | IFUSE\tX\tERR\t<code>
//   IFUSE\tO                               open-window push (gossip bridge)

namespace mod_item_infusion::addon
{

constexpr std::string_view PREFIX = "IFUSE";
constexpr size_t MAX_PAYLOAD = 255;

struct Request
{
    enum class Kind : uint8
    {
        Invalid = 0,
        Preview,
        Execute
    };

    Kind kind = Kind::Invalid;
    uint32 targetInvSlot = 0; // raw client value (1-19)
    uint32 donorBag = 0;      // raw client value (0-4)
    uint32 donorSlot = 0;     // raw client value (1-based)
    uint32 coins = 0;
    uint32 substanceMask = 0;
};

struct YieldView
{
    uint8 property;
    int32 amount;
};

struct SubstanceView
{
    uint32 index;
    uint32 itemId;
    uint32 reductionPct;
    bool eligible;
};

// True if msg is addressed to this module ("IFUSE\t...").
bool IsAddonMessage(std::string_view msg);

// Parses a full incoming message (including prefix). Returns Kind::Invalid
// on any malformed input; never throws.
Request ParseRequest(std::string_view msg);

// Reply builders. BuildYield/BuildSubstances chunk whole entries to the
// payload cap and may return MULTIPLE messages.
std::string BuildHeader(uint32 riskPct, uint32 basePct, uint32 penaltyPct,
                        uint32 mixPts, uint32 nativePts);
std::vector<std::string> BuildYield(std::vector<YieldView> const& yield);
std::vector<std::string> BuildSubstances(std::vector<SubstanceView> const& subs);
std::string BuildRefusal(std::string_view code);
std::string BuildExecuteResult(std::string_view code); // OK / DEAD / ERR:<code>
std::string BuildOpen();

} // namespace mod_item_infusion::addon

#endif // MOD_ITEM_INFUSION_ADDON_MSG_H
