#include "PropertyOverrideAddonMsg.h"

#include <charconv>

namespace mod_property_override::addon
{

namespace
{
    // Splits msg on '\t' into at most maxParts tokens (the last token keeps
    // any remaining tabs — callers only split as deep as the grammar goes).
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
}

bool IsAddonMessage(std::string_view msg)
{
    return msg.size() > PREFIX.size()
        && msg.substr(0, PREFIX.size()) == PREFIX
        && msg[PREFIX.size()] == '\t';
}

Query ParseQuery(std::string_view msg)
{
    Query q;
    if (!IsAddonMessage(msg) || msg.size() > MAX_PAYLOAD)
        return q;

    std::string_view body = msg.substr(PREFIX.size() + 1);
    std::vector<std::string_view> parts = SplitTabs(body, 4);

    if (parts.size() < 3 || parts[0] != "Q")
        return q;

    if (parts[1] == "E")
    {
        if (parts.size() != 3 || !ParseU32(parts[2], q.slot))
            return q;
        q.kind = Query::Kind::Equipped;
        return q;
    }

    if (parts[1] == "B")
    {
        if (parts.size() != 4 || !ParseU32(parts[2], q.bag) || !ParseU32(parts[3], q.slot))
            return q;
        q.kind = Query::Kind::Bag;
        return q;
    }

    return q;
}

std::string BuildReply(Query const& query, std::vector<RowView> const& rows)
{
    std::string reply;
    reply.reserve(64);
    reply.append(PREFIX).append("\tR\t");
    if (query.kind == Query::Kind::Equipped)
        reply.append("E\t").append(std::to_string(query.slot));
    else
        reply.append("B\t").append(std::to_string(query.bag))
             .append("\t").append(std::to_string(query.slot));
    reply.push_back('\t');

    if (rows.empty())
    {
        reply.push_back('-');
        return reply;
    }

    bool first = true;
    for (RowView const& row : rows)
    {
        std::string entry;
        if (!first)
            entry.push_back(';');
        entry.append(std::to_string(row.property))
             .push_back(':');
        entry.append(std::to_string(row.value))
             .push_back(':');
        entry.append(std::to_string(row.expiry));

        if (reply.size() + entry.size() > MAX_PAYLOAD)
            break;
        reply.append(entry);
        first = false;
    }

    if (first) // not even one row fit
        reply.push_back('-');
    return reply;
}

std::string BuildInvalidate()
{
    std::string msg;
    msg.append(PREFIX).append("\tI");
    return msg;
}

} // namespace mod_property_override::addon
