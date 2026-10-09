
#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "cache_test_cases.hpp"

class CacheTest : public testing::TestWithParam<CacheImplementation>
{
protected:
    std::unique_ptr<TestCache> cache;

    void SetUp() override
    {
        cache = GetParam().create(10);
        ASSERT_NE(cache, nullptr);
    }
};

TEST_P(CacheTest, FirstRequestIsMiss)
{
    EXPECT_TRUE(cache->Request(1));
}

TEST_P(CacheTest, RepeatedRequestIsHit)
{
    EXPECT_TRUE(cache->Request(1));
    EXPECT_FALSE(cache->Request(1));
}

TEST_P(CacheTest, UniqueRequestsAreMisses)
{
    for (int key = 1; key <= 5; ++key)
    {
        EXPECT_TRUE(cache->Request(key))
            << "Key: " << key;
    }
}

TEST_P(CacheTest, LoadedKeysAreHits)
{
    for (int key = 1; key <= 5; ++key)
    {
        EXPECT_TRUE(cache->Request(key));
    }

    for (int key = 1; key <= 5; ++key)
    {
        EXPECT_FALSE(cache->Request(key))
            << "Key: " << key;
    }
}

TEST_P(CacheTest, RunCacheCountsMisses)
{
    std::vector<int> requests = {
        1, 2, 3, 1, 2, 3
    };

    EXPECT_EQ(
        RunCache(cache.get(), requests.size(), requests),
        3
    );
}

TEST_P(CacheTest, RepeatedKeyHasOneMiss)
{
    std::vector<int> requests = {
        42, 42, 42, 42, 42
    };

    EXPECT_EQ(
        RunCache(cache.get(), requests.size(), requests),
        1
    );
}

INSTANTIATE_TEST_SUITE_P(
    AllCacheAlgorithms,
    CacheTest,
    testing::ValuesIn(GetCacheImplementations()),
    [](const testing::TestParamInfo<CacheImplementation>& info)
    {
        return info.param.name;
    }
);