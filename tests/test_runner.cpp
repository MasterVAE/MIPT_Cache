#include <gtest/gtest.h>
#include "tests.hpp"

int RunUnitTests()
{
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}