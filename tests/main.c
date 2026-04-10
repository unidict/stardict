//
//  main.c
//  stardict tests
//
//  Unity-based test runner for libstardict
//

#include "unity.h"
#include <stdio.h>

// Test suite declarations
void run_array_tests(void);
void run_dictzip_tests(void);
void run_stardict_tests(void);
void run_dictd_tests(void);

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("  stardict Test Suite (Unity Framework)\n");
    printf("========================================\n\n");

    run_array_tests();
    run_dictzip_tests();
    run_stardict_tests();
    run_dictd_tests();

    printf("\n");
    printf("========================================\n");
    printf("  Test Summary\n");
    printf("========================================\n");

    return UnityEnd();
}
