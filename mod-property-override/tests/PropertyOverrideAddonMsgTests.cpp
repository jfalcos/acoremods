#include "PropertyOverrideAddonMsg.h"

#include <gtest/gtest.h>

using namespace mod_property_override::addon;

TEST(AddonMsg, DetectsPrefix)
{
    EXPECT_TRUE(IsAddonMessage("IPROP\tQ\tE\t5"));
    EXPECT_FALSE(IsAddonMessage("IPROP"));
    EXPECT_FALSE(IsAddonMessage("IPROPX\tQ"));
    EXPECT_FALSE(IsAddonMessage("iprop\tQ"));
    EXPECT_FALSE(IsAddonMessage(""));
    EXPECT_FALSE(IsAddonMessage("hello world"));
}

TEST(AddonMsg, ParsesEquippedQuery)
{
    Query q = ParseQuery("IPROP\tQ\tE\t16");
    ASSERT_EQ(q.kind, Query::Kind::Equipped);
    EXPECT_EQ(q.slot, 16u);
}

TEST(AddonMsg, ParsesBagQuery)
{
    Query q = ParseQuery("IPROP\tQ\tB\t2\t14");
    ASSERT_EQ(q.kind, Query::Kind::Bag);
    EXPECT_EQ(q.bag, 2u);
    EXPECT_EQ(q.slot, 14u);
}

TEST(AddonMsg, RejectsMalformedInput)
{
    char const* bad[] =
    {
        "IPROP\t",                    // empty body
        "IPROP\tQ",                   // no target
        "IPROP\tQ\tE",                // no slot
        "IPROP\tQ\tE\t",              // empty slot
        "IPROP\tQ\tE\tabc",           // non-numeric
        "IPROP\tQ\tE\t-3",            // negative
        "IPROP\tQ\tE\t5\t9",          // trailing junk on E
        "IPROP\tQ\tB\t2",             // bag without slot
        "IPROP\tQ\tB\t2\t3\t4",       // trailing junk on B
        "IPROP\tQ\tX\t5",             // unknown target
        "IPROP\tR\tE\t5\t-",          // reply shape, not a query
        "IPROP\tQ\tE\t99999999999",   // overflow
        "IPROP\tQ\tE\t5x",            // trailing garbage in number
    };
    for (char const* msg : bad)
        EXPECT_EQ(ParseQuery(msg).kind, Query::Kind::Invalid) << msg;
}

TEST(AddonMsg, RejectsOversizedMessage)
{
    std::string msg = "IPROP\tQ\tE\t5";
    msg.append(300, 'a');
    EXPECT_EQ(ParseQuery(msg).kind, Query::Kind::Invalid);
}

TEST(AddonMsg, BuildsEmptyReplyEchoingCoords)
{
    Query q;
    q.kind = Query::Kind::Equipped;
    q.slot = 16;
    EXPECT_EQ(BuildReply(q, {}), "IPROP\tR\tE\t16\t-");

    Query b;
    b.kind = Query::Kind::Bag;
    b.bag = 3;
    b.slot = 7;
    EXPECT_EQ(BuildReply(b, {}), "IPROP\tR\tB\t3\t7\t-");
}

TEST(AddonMsg, BuildsRowReply)
{
    Query q;
    q.kind = Query::Kind::Equipped;
    q.slot = 4;
    std::vector<RowView> rows = { { 2, 50, 0 }, { 0, -10, 12345 } };
    EXPECT_EQ(BuildReply(q, rows), "IPROP\tR\tE\t4\t2:50:0;0:-10:12345");
}

TEST(AddonMsg, ReplyStaysWithinPayloadCap)
{
    Query q;
    q.kind = Query::Kind::Equipped;
    q.slot = 1;
    // Far more rows than can fit; whole rows are dropped, never truncated.
    std::vector<RowView> rows;
    for (int i = 0; i < 100; ++i)
        rows.push_back({ 4, 2000000000, 18446744073709551615ull });
    std::string reply = BuildReply(q, rows);
    EXPECT_LE(reply.size(), MAX_PAYLOAD);
    EXPECT_EQ(reply.back(), '5'); // ends on a complete row, not mid-field
}

TEST(AddonMsg, InvalidateShape)
{
    EXPECT_EQ(BuildInvalidate(), "IPROP\tI");
}
