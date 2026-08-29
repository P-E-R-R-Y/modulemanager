#include <gtest/gtest.h>

#include "Stride.hpp"

namespace {
struct A { std::string v; };
struct B { std::string v; };
struct C { std::string v; };
struct D { std::string v; };
} // namespace

TEST(Stride, AddReturnsDistinctEntities) {
    Stride<int, A, B> t;
    auto e0 = t.add(0);
    auto e1 = t.add(1);
    EXPECT_FALSE(e0 == e1);
}

TEST(Stride, IdentityRoundTrip) {
    Stride<std::string, A> t;
    auto e = t.add("owner");
    ASSERT_TRUE(t.identity(e).has_value());
    EXPECT_EQ(*t.identity(e), "owner");
}

TEST(Stride, SetAndAtRoundTrip) {
    Stride<int, A, B> t;
    auto e = t.add(0);
    t.set<A>(e, A{"hello"});
    ASSERT_TRUE(t.at<A>(e).has_value());
    EXPECT_EQ(t.at<A>(e)->v, "hello");
    EXPECT_FALSE(t.at<B>(e).has_value()); /* never set: a hole, not an error */
}

TEST(Stride, ColumnIsTheWholeColumnNoCopy) {
    Stride<int, A, B, C, D> t;
    auto r0 = t.add(0);
    t.add(1); /* r1 : left empty on purpose, a hole in the middle */
    auto r2 = t.add(2);
    t.set<B>(r0, B{"r0"});
    t.set<B>(r2, B{"r2"});

    auto &colB = t.column<B>();
    ASSERT_EQ(colB.size(), 3u);
    EXPECT_EQ(colB[0]->v, "r0");
    EXPECT_FALSE(colB[1].has_value());
    EXPECT_EQ(colB[2]->v, "r2");
}

TEST(Stride, RowVisitsEveryColumn) {
    Stride<int, A, B, C> t;
    auto r = t.add(0);
    t.set<A>(r, A{"a"});
    t.set<C>(r, C{"c"});

    int seen = 0;
    t.row(r, [&](auto &slot) {
        if (slot)
            ++seen;
    });
    EXPECT_EQ(seen, 2); /* A and C, not B */
}

TEST(Stride, RemoveClearsTheRowFreesTheIndexAndDropsTheIdentity) {
    Stride<int, A, B> t;
    auto r = t.add(42);
    t.set<A>(r, A{"x"});
    t.remove(r);
    EXPECT_FALSE(t.at<A>(r).has_value());
    EXPECT_FALSE(t.identity(r).has_value());

    auto reused = t.add(7);
    EXPECT_TRUE(reused == r); /* the index is recycled, like ecs Entity */
    EXPECT_EQ(*t.identity(reused), 7);
}

TEST(Stride, SizeCountsAllocatedRows) {
    Stride<int, A, B> t;
    EXPECT_EQ(t.size(), 0u);
    t.add(0);
    t.add(1);
    EXPECT_EQ(t.size(), 2u);
}
