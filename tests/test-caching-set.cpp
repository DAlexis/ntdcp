#include "ntdcp/caching-set.hpp"

#include "gtest/gtest.h"

TEST(CachingSet, Operating)
{
    CachingSet<int> cs(4);
    ASSERT_FALSE(cs.check_update(1));
    ASSERT_TRUE(cs.check_update(1));

    ASSERT_FALSE(cs.check_update(2));
    ASSERT_TRUE(cs.check_update(2));
    ASSERT_TRUE(cs.check_update(1));

    ASSERT_FALSE(cs.check_update(3));
    ASSERT_TRUE(cs.check_update(3));
    ASSERT_TRUE(cs.check_update(1));

    ASSERT_FALSE(cs.check_update(4));
    ASSERT_TRUE(cs.check_update(4));
    ASSERT_TRUE(cs.check_update(1));

    ASSERT_FALSE(cs.check_update(5));
    ASSERT_TRUE(cs.check_update(1));

    // "2" should be already displaced
    ASSERT_FALSE(cs.check_update(2));
    ASSERT_TRUE(cs.check_update(2));
}


TEST(CachingMap, Operating)
{
    CachingMap<int, int> m(5);
    ASSERT_FALSE(m.get_update(1).has_value());
    ASSERT_FALSE(m.put_update(1, 10));
    ASSERT_FALSE(m.put_update(2, 20));
    ASSERT_FALSE(m.put_update(3, 30));
    ASSERT_FALSE(m.put_update(4, 40));
    ASSERT_FALSE(m.put_update(5, 50));
    ASSERT_FALSE(m.put_update(6, 60));

    ASSERT_TRUE(m.get(6).has_value());
    ASSERT_TRUE(m.get(5).has_value());
    ASSERT_TRUE(m.get(4).has_value());
    ASSERT_TRUE(m.get(3).has_value());
    ASSERT_TRUE(m.get(2).has_value());

    ASSERT_FALSE(m.get(1).has_value());

    ASSERT_TRUE(m.get_update(2).has_value());
    ASSERT_FALSE(m.put_update(7, 70));

    ASSERT_TRUE(m.get(2).has_value());
    ASSERT_FALSE(m.get(3).has_value());
}
