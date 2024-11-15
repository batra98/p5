// whatever the header file name is, here it should be tester.h,
// because the header file is copied as "tester.h" during testing
#include "tester.h"

// ====================================================================
// TEST_2
// Summary: Checks the presence of Segmentation Fault
// ====================================================================

char *test_name = "TEST_2";

int main() {
    printf(1, "\n\n%s\n", test_name);

    int addr = KERNBASE + 1;
    char *arr = (char *)addr;

    printf(1, "I am here 1\n");
    arr[0] = 'a'; // this should cause a segfault
    printf(1, "I am here 2\n");
    // test ends
    success();
}
