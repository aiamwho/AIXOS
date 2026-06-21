#include "test.h"
#include <stdio.h>

int test_failures;
int test_checks;

void test_mpu_regions(void);
void test_user_copy_api(void);

int main(void)
{
    RUN_TEST(test_mpu_regions);
    RUN_TEST(test_user_copy_api);
    printf("%d checks, %d failures\n", test_checks, test_failures);
    return test_failures ? 1 : 0;
}
