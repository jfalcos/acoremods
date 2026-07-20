#include "ItemInfusionAddonMsg.h"

#include <charconv>

namespace mod_item_infusion::addon
{

namespace
{
    std::vector<std::string_view> SplitTabs(std::string_view msg, size_t maxParts)
    {
        std::vector<std::string_view> parts;
        while (parts.size() + 1 < maxParts)
        {
            size_t pos = msg.find('\t');
            if (pos == std::string_view::npos)
                break;
            parts.push_back(msg.substr(0, pos));
            msg.remove_prefix(pos + 1);
        }
        parts.push_back(msg);
        return parts;
    }

    bool ParseU32(std::string_view token, uint32& out)
    {
        if (token.empty() || token.size() > 10)
            return false;
        auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), out);
        return ec == std::errc() && ptr == token.data() + token.size();
    }

    // Appends semicolon-joined entries to "IFUSE\tR\t<tag>\t" messages,
    // starting a new message whenever the next whole entry would overflow.
    std::vector<std::string> ChunkEntries(std::string_view tag,
                                          std::vector<std::string> const& entries)
    {
        std::vector<std::string> out;
        std::string head;
        head.append(PREFIX).append("\tR\t").append(tag).push_back('\t');

        std::string cur = head;
        bool any = false;
        for (std::string const& entry : entries)
        {
            size_t addition = entry.size() + (any ? 1 : 0);
            if (cur.size() + addition > MAX_PAYLOAD)
            {
                if (any)
                    out.push_back(cur);
                cur = head;
                any = false;
            }
            if (any)
                cur.push_back(';');
            cur.append(entry);
            any = true;
        }
        if (any)
            out.push_back(cur);
        return out;
    }
}

bool IsAddonMessage(std::string_view msg)
{
    return msg.size() > PREFIX.size()
        && msg.substr(0, PREFIX.size()) == PREFIX
        && msg[PREFIX.size()] == '\t';
}

Request ParseRequest(std::string_view msg)
{
    Request r;
    if (!IsAddonMessage(msg) || msg.size() > MAX_PAYLOAD)
        return r;

    std::string_view body = msg.substr(PREFIX.size() + 1);
    std::vector<std::string_view> parts = SplitTabs(body, 7);
    if (parts.size() != 6)
        return r;

    Request::Kind kind;
    if (parts[0] == "P")
        kind = Request::Kind::Preview;
    else if (parts[0] == "X")
        kind = Request::Kind::Execute;
    else
        return r;

    if (!ParseU32(parts[1], r.targetInvSlot) || !ParseU32(parts[2], r.donorBag) ||
        !ParseU32(parts[3], r.donorSlot) || !ParseU32(parts[4], r.coins) ||
        !ParseU32(parts[5], r.substanceMask))
        return r;

    r.kind = kind;
    return r;
}

std::string BuildHeader(uint32 riskPct, uint32 basePct, uint32 penaltyPct,
                        uint32 mixPts, uint32 nativePts)
{
    std::string msg;
    msg.reserve(64);
    msg.append(PREFIX).append("\tR\tH\t")
       .append(std::to_string(riskPct)).append("\t")
       .append(std::to_string(basePct)).append("\t")
       .append(std::to_string(penaltyPct)).append("\t")
       .append(std::to_string(mixPts)).append("\t")
       .append(std::to_string(nativePts));
    return msg;
}

std::vector<std::string> BuildYield(std::vector<YieldView> const& yield)
{
    std::vector<std::string> entries;
    entries.reserve(yield.size());
    for (YieldView const& y : yield)
        entries.push_back(std::to_string(y.property) + ":" + std::to_string(y.amount));
    return ChunkEntries("Y", entries);
}

std::vector<std::string> BuildSubstances(std::vector<SubstanceView> const& subs)
{
    std::vector<std::string> entries;
    entries.reserve(subs.size());
    for (SubstanceView const& s : subs)
        entries.push_back(std::to_string(s.index) + ":" + std::to_string(s.itemId) +
                          ":" + std::to_string(s.reductionPct) +
                          ":" + (s.eligible ? "1" : "0"));
    return ChunkEntries("S", entries);
}

std::string BuildRefusal(std::string_view code)
{
    std::string msg;
    msg.append(PREFIX).append("\tR\tE\t").append(code);
    return msg;
}

std::string BuildExecuteResult(std::string_view code)
{
    std::string msg;
    msg.append(PREFIX).append("\tX\t").append(code);
    return msg;
}

std::string BuildOpen()
{
    std::string msg;
    msg.append(PREFIX).append("\tO");
    return msg;
}

} // namespace mod_item_infusion::addon
