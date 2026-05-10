#include <stdint.h>
#include <stdio.h>

#include "poly_helpers.h"

#define TEST_WORDS 8u

static int test_zero_polynomial(void)
{
    uint64_t a[TEST_WORDS] = {0xFFFFFFFFFFFFFFFFULL};

    hqc_poly_zero(a, TEST_WORDS);
    if (!hqc_poly_is_zero(a, TEST_WORDS)) {
        fprintf(stderr, "zero polynomial test failed\n");
        return 1;
    }
    return 0;
}

static int test_identity_polynomial(void)
{
    uint64_t a[TEST_WORDS] = {0};

    hqc_poly_identity(a, TEST_WORDS);
    if (!hqc_poly_is_identity(a, TEST_WORDS)) {
        fprintf(stderr, "identity polynomial test failed\n");
        return 1;
    }
    return 0;
}

static int test_identity_with_zero_words(void)
{
    uint64_t dummy = 0xABCDULL;

    hqc_poly_identity(&dummy, 0u);
    if (dummy != 0xABCDULL) {
        fprintf(stderr, "identity(0 words) modified memory\n");
        return 1;
    }
    return 0;
}

static int test_xor_helper(void)
{
    uint64_t a[TEST_WORDS] = {1ULL, 2ULL, 3ULL, 4ULL, 5ULL, 6ULL, 7ULL, 8ULL};
    uint64_t b[TEST_WORDS] = {1ULL, 0ULL, 3ULL, 0ULL, 5ULL, 0ULL, 7ULL, 0ULL};

    hqc_poly_xor(a, b, TEST_WORDS);

    if (a[0] != 0ULL || a[1] != 2ULL || a[2] != 0ULL || a[3] != 4ULL ||
        a[4] != 0ULL || a[5] != 6ULL || a[6] != 0ULL || a[7] != 8ULL) {
        fprintf(stderr, "xor helper test failed\n");
        return 1;
    }
    return 0;
}

int main(void)
{
    int rc = 0;

    rc |= test_zero_polynomial();
    rc |= test_identity_polynomial();
    rc |= test_identity_with_zero_words();
    rc |= test_xor_helper();

    if (rc == 0) {
        puts("poly_helpers: PASS");
    }
    return rc;
}
