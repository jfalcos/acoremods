#include "PropertyOverrideMgr.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace mod_property_override;

namespace
{
    bool Contains(std::vector<ObjectGuid::LowType> const& v, ObjectGuid::LowType g)
    {
        return std::find(v.begin(), v.end(), g) != v.end();
    }

    AppliedItem MakeApplied(std::vector<AppliedRow> rows, uint64 minExpiry = 0)
    {
        AppliedItem item;
        item.rows = std::move(rows);
        item.minExpiry = minExpiry;
        return item;
    }
}

TEST(Lifecycle, FreshEquipApplies)
{
    ItemOverrideMap overrides;
    overrides[100] = { { 2, 50, 0 } };
    AppliedMap applied;

    SyncActions a = ComputeSyncActions(overrides, applied, { 100 }, 1000);
    EXPECT_TRUE(a.unapply.empty());
    EXPECT_TRUE(a.expired.empty());
    ASSERT_EQ(a.apply.size(), 1u);
    EXPECT_EQ(a.apply[0], 100u);
}

TEST(Lifecycle, UnequipUnapplies)
{
    ItemOverrideMap overrides;
    overrides[100] = { { 2, 50, 0 } };
    AppliedMap applied;
    applied[100] = MakeApplied({ { 2, 50.f } });

    SyncActions a = ComputeSyncActions(overrides, applied, {}, 1000);
    ASSERT_EQ(a.unapply.size(), 1u);
    EXPECT_EQ(a.unapply[0], 100u);
    EXPECT_TRUE(a.apply.empty());
    EXPECT_TRUE(a.expired.empty());
}

TEST(Lifecycle, SteadyStateIsANoOp)
{
    ItemOverrideMap overrides;
    overrides[100] = { { 2, 50, 0 } };
    overrides[200] = { { 0, 10, 0 } };
    AppliedMap applied;
    applied[100] = MakeApplied({ { 2, 50.f } });
    applied[200] = MakeApplied({ { 0, 10.f } });

    SyncActions a = ComputeSyncActions(overrides, applied, { 100, 200 }, 1000);
    EXPECT_TRUE(a.unapply.empty());
    EXPECT_TRUE(a.apply.empty());
    EXPECT_TRUE(a.expired.empty());
}

TEST(Lifecycle, SwapUnappliesOldAndAppliesNew)
{
    ItemOverrideMap overrides;
    overrides[100] = { { 2, 50, 0 } };
    overrides[200] = { { 0, 10, 0 } };
    AppliedMap applied;
    applied[100] = MakeApplied({ { 2, 50.f } });

    SyncActions a = ComputeSyncActions(overrides, applied, { 200 }, 1000);
    ASSERT_EQ(a.unapply.size(), 1u);
    EXPECT_EQ(a.unapply[0], 100u);
    ASSERT_EQ(a.apply.size(), 1u);
    EXPECT_EQ(a.apply[0], 200u);
}

TEST(Lifecycle, ExpiredWhileEquippedReappliesRemainingRows)
{
    // Item has one expired row and one live row: old snapshot must come off
    // (unapply), expired rows pruned, remaining row reapplied (apply).
    ItemOverrideMap overrides;
    overrides[100] = { { 2, 50, 500 }, { 0, 10, 0 } };
    AppliedMap applied;
    applied[100] = MakeApplied({ { 2, 50.f }, { 0, 10.f } }, /*minExpiry=*/500);

    SyncActions a = ComputeSyncActions(overrides, applied, { 100 }, 1000);
    EXPECT_TRUE(Contains(a.expired, 100));
    EXPECT_TRUE(Contains(a.unapply, 100));
    EXPECT_TRUE(Contains(a.apply, 100));
}

TEST(Lifecycle, FullyExpiredItemOnlyUnapplies)
{
    ItemOverrideMap overrides;
    overrides[100] = { { 2, 50, 500 } };
    AppliedMap applied;
    applied[100] = MakeApplied({ { 2, 50.f } }, /*minExpiry=*/500);

    SyncActions a = ComputeSyncActions(overrides, applied, { 100 }, 1000);
    EXPECT_TRUE(Contains(a.expired, 100));
    EXPECT_TRUE(Contains(a.unapply, 100));
    EXPECT_TRUE(a.apply.empty()); // no live rows left -> not desired
}

TEST(Lifecycle, NotYetExpiredIsANoOp)
{
    ItemOverrideMap overrides;
    overrides[100] = { { 2, 50, 2000 } };
    AppliedMap applied;
    applied[100] = MakeApplied({ { 2, 50.f } }, /*minExpiry=*/2000);

    SyncActions a = ComputeSyncActions(overrides, applied, { 100 }, 1000);
    EXPECT_TRUE(a.unapply.empty());
    EXPECT_TRUE(a.apply.empty());
    EXPECT_TRUE(a.expired.empty());
}

TEST(Lifecycle, EquippedItemWithOnlyExpiredRowsNeverApplies)
{
    ItemOverrideMap overrides;
    overrides[100] = { { 2, 50, 500 } };
    AppliedMap applied; // never applied (e.g. expired while in bags)

    SyncActions a = ComputeSyncActions(overrides, applied, { 100 }, 1000);
    EXPECT_TRUE(a.unapply.empty());
    EXPECT_TRUE(a.apply.empty());
    EXPECT_TRUE(a.expired.empty());
}

TEST(Lifecycle, IdempotentAfterActionsAreExecuted)
{
    ItemOverrideMap overrides;
    overrides[100] = { { 2, 50, 0 } };
    AppliedMap applied;

    SyncActions first = ComputeSyncActions(overrides, applied, { 100 }, 1000);
    ASSERT_EQ(first.apply.size(), 1u);

    // Simulate the executor: apply -> snapshot recorded.
    applied[100] = MakeApplied({ { 2, 50.f } });

    SyncActions second = ComputeSyncActions(overrides, applied, { 100 }, 1000);
    EXPECT_TRUE(second.unapply.empty());
    EXPECT_TRUE(second.apply.empty());
    EXPECT_TRUE(second.expired.empty());
}
