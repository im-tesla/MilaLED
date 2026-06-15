#include <unity.h>
#include "../../src/leds/PixelMapper.h"

void setUp() {}
void tearDown() {}

// Config: segA=4 physical (half density), segB=3 physical
// Virtual: 2 from segA (skip-every-other) + 3 from segB = 5 virtual total

void test_segA_virtual_to_physical() {
    PixelMapper pm(4, true, 3);
    // virtual 0 → physical 0, virtual 1 → physical 2
    TEST_ASSERT_EQUAL(0, pm.toPhysical(0));
    TEST_ASSERT_EQUAL(2, pm.toPhysical(1));
}

void test_segB_virtual_to_physical() {
    PixelMapper pm(4, true, 3);
    // virtual 2 → physical 4 (segB start), virtual 3 → 5, virtual 4 → 6
    TEST_ASSERT_EQUAL(4, pm.toPhysical(2));
    TEST_ASSERT_EQUAL(5, pm.toPhysical(3));
    TEST_ASSERT_EQUAL(6, pm.toPhysical(4));
}

void test_virtual_count_half_density() {
    PixelMapper pm(4, true, 3);
    TEST_ASSERT_EQUAL(5, pm.virtualCount());
}

void test_full_density_passthrough() {
    PixelMapper pm(4, false, 3);
    // no skipping: virtual 0→0, virtual 1→1, virtual 2→2, segB at 3
    TEST_ASSERT_EQUAL(0, pm.toPhysical(0));
    TEST_ASSERT_EQUAL(1, pm.toPhysical(1));
    TEST_ASSERT_EQUAL(4, pm.toPhysical(4)); // virtual 4 → physical 4
    TEST_ASSERT_EQUAL(7, pm.virtualCount());
}

void test_clamps_oob() {
    PixelMapper pm(4, true, 3);
    // out-of-range virtual index returns last physical index
    TEST_ASSERT_EQUAL(6, pm.toPhysical(99));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_segA_virtual_to_physical);
    RUN_TEST(test_segB_virtual_to_physical);
    RUN_TEST(test_virtual_count_half_density);
    RUN_TEST(test_full_density_passthrough);
    RUN_TEST(test_clamps_oob);
    return UNITY_END();
}
