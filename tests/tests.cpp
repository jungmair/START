#include <gtest/gtest.h>
//execute all defined tests: "normal"+"range"
int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}