#include "ntdcp/caching-set.hpp"

#include "gtest/gtest.h"

TEST(CachingSet, Operating)
{
    std::list<int>::iterator it;
    std::set<std::list<int>::iterator> s;
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
