#include <gtest/gtest.h>

#include "ItemInfusionAddonMsg.h"

using namespace mod_item_infusion::addon;

TEST(IfuseMsg, ParsesPreviewAndExecute)
{
    Request p = ParseRequest("IFUSE\tP\t16\t0\t3\t2\t5");
    EXPECT_EQ(p.kind, Request::Kind::Preview);
    EXPECT_EQ(p.targetInvSlot, 16u);
    EXPECT_EQ(p.donorBag, 0u);
    EXPECT_EQ(p.donorSlot, 3u);
    EXPECT_EQ(p.coins, 2u);
    EXPECT_EQ(p.substanceMask, 5u);

    Request x = ParseRequest("IFUSE\tX\t16\t4\t18\t0\t0");
    EXPECT_EQ(x.kind, Request::Kind::Execute);
    EXPECT_EQ(x.donorBag, 4u);
}

TEST(IfuseMsg, MalformedInputNeverParses)
{
    char const* bad[] =
    {
        "", "IFUSE", "IFUSE\t", "IPROP\tP\t1\t0\t1\t0\t0",
        "IFUSE\tP\t1\t0\t1\t0",           // too few fields
        "IFUSE\tP\t1\t0\t1\t0\t0\t9",     // too many fields
        "IFUSE\tZ\t1\t0\t1\t0\t0",        // unknown verb
        "IFUSE\tP\tx\t0\t1\t0\t0",        // non-numeric
        "IFUSE\tP\t-1\t0\t1\t0\t0",       // negative
        "IFUSE\tP\t99999999999\t0\t1\t0\t0", // overlong number
    };
    for (char const* msg : bad)
        EXPECT_EQ(ParseRequest(msg).kind, Request::Kind::Invalid) << msg;
}

TEST(IfuseMsg, HeaderAndSimpleBuilders)
{
    EXPECT_EQ(BuildHeader(23, 5, 30, 84, 240), "IFUSE\tR\tH\t23\t5\t30\t84\t240");
    EXPECT_EQ(BuildRefusal("LEVEL"), "IFUSE\tR\tE\tLEVEL");
    EXPECT_EQ(BuildExecuteResult("DEAD"), "IFUSE\tX\tDEAD");
    EXPECT_EQ(BuildOpen(), "IFUSE\tO");
}

TEST(IfuseMsg, YieldChunksToPayloadCap)
{
    // 60 entries of "104:100000" (10 chars) blow well past 255 bytes.
    std::vector<YieldView> yield;
    for (int i = 0; i < 60; ++i)
        yield.push_back({ 104, 100000 });
    auto msgs = BuildYield(yield);
    ASSERT_TRUE(msgs.size() > 1u);
    size_t entries = 0;
    for (auto const& m : msgs)
    {
        EXPECT_LE(m.size(), MAX_PAYLOAD);
        EXPECT_EQ(m.substr(0, 10), "IFUSE\tR\tY\t");
        for (size_t pos = m.find(':'); pos != std::string::npos; pos = m.find(':', pos + 1))
            ++entries;
    }
    EXPECT_EQ(entries, 60u); // every entry survives chunking exactly once
    EXPECT_TRUE(BuildYield({}).empty());
}

TEST(IfuseMsg, SubstancesEncodeEligibility)
{
    auto msgs = BuildSubstances({ { 0, 118, 5, true }, { 3, 35625, 20, false } });
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0], "IFUSE\tR\tS\t0:118:5:1;3:35625:20:0");
}
