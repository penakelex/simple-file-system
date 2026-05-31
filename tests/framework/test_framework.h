#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static uint32_t framework_total_tests = 0;
static uint32_t framework_passed_tests = 0;
static uint32_t framework_failed_tests = 0;

#define TEST_ASSERT(condition, message)                    \
  do {                                                     \
    framework_total_tests++;                               \
    if (condition) {                                       \
      framework_passed_tests++;                            \
    } else {                                               \
      framework_failed_tests++;                            \
      fprintf(stderr,                                      \
              "  FAIL: %s (line %d): %s\n",                \
              __func__,                                    \
              __LINE__,                                    \
              (message));                                  \
    }                                                      \
  } while (0)

#define TEST_ASSERT_EQUAL_INT(expected, actual, message)   \
  do {                                                     \
    framework_total_tests++;                               \
    if ((expected) == (actual)) {                          \
      framework_passed_tests++;                            \
    } else {                                               \
      framework_failed_tests++;                            \
      fprintf(stderr,                                      \
              "  FAIL: %s (line %d): %s (expected %d, "    \
              "got %d)\n",                                 \
              __func__,                                    \
              __LINE__,                                    \
              (message),                                   \
              (int)(expected),                             \
              (int)(actual));                              \
    }                                                      \
  } while (0)

#define TEST_ASSERT_EQUAL_UINT(expected, actual, message)  \
  do {                                                     \
    framework_total_tests++;                               \
    if ((expected) == (actual)) {                          \
      framework_passed_tests++;                            \
    } else {                                               \
      framework_failed_tests++;                            \
      fprintf(stderr,                                      \
              "  FAIL: %s (line %d): %s (expected %u, "    \
              "got %u)\n",                                 \
              __func__,                                    \
              __LINE__,                                    \
              (message),                                   \
              (unsigned)(expected),                        \
              (unsigned)(actual));                         \
    }                                                      \
  } while (0)

#define TEST_ASSERT_EQUAL_INT64(expected, actual, message) \
  do {                                                     \
    framework_total_tests++;                               \
    if ((expected) == (actual)) {                          \
      framework_passed_tests++;                            \
    } else {                                               \
      framework_failed_tests++;                            \
      fprintf(stderr,                                      \
              "  FAIL: %s (line %d): %s (expected %lld, "  \
              "got %lld)\n",                               \
              __func__,                                    \
              __LINE__,                                    \
              (message),                                   \
              (long long)(expected),                       \
              (long long)(actual));                        \
    }                                                      \
  } while (0)

#define TEST_ASSERT_EQUAL_SIZE(expected, actual, message)  \
  do {                                                     \
    framework_total_tests++;                               \
    if ((expected) == (actual)) {                          \
      framework_passed_tests++;                            \
    } else {                                               \
      framework_failed_tests++;                            \
      fprintf(stderr,                                      \
              "  FAIL: %s (line %d): %s (expected %zu, "   \
              "got %zu)\n",                                \
              __func__,                                    \
              __LINE__,                                    \
              (message),                                   \
              (size_t)(expected),                          \
              (size_t)(actual));                           \
    }                                                      \
  } while (0)

#define TEST_ASSERT_EQUAL_STRING(                          \
  expected, actual, message)                               \
  do {                                                     \
    framework_total_tests++;                               \
    if (strcmp((expected), (actual)) == 0) {               \
      framework_passed_tests++;                            \
    } else {                                               \
      framework_failed_tests++;                            \
      fprintf(stderr,                                      \
              "  FAIL: %s (line %d): %s (expected "        \
              "\"%s\", got \"%s\")\n",                     \
              __func__,                                    \
              __LINE__,                                    \
              (message),                                   \
              (expected),                                  \
              (actual));                                   \
    }                                                      \
  } while (0)

#define TEST_ASSERT_STATUS_OK(status, message)             \
  TEST_ASSERT_EQUAL_INT(FS_STATUS_OK, (status), (message))

#define TEST_ASSERT_STATUS_EQUAL(                          \
  expected, actual, message)                               \
  TEST_ASSERT_EQUAL_INT((expected), (actual), (message))

#define TEST_RUN(test_function)                            \
  do {                                                     \
    printf("  Running %s...\n", #test_function);           \
    test_function();                                       \
  } while (0)

#define TEST_SUITE_BEGIN(suite_name)                       \
  printf("\nTest Suite: %s\n", (suite_name))

#define TEST_SUITE_END()                                   \
  do {                                                     \
    printf("\nResults:\n");                         \
    printf("Total: %u | Passed: %u | Failed: %u\n",        \
           framework_total_tests,                          \
           framework_passed_tests,                         \
           framework_failed_tests);                        \
  } while (0)

#define TEST_EXIT()                                        \
  return (framework_failed_tests > 0) ? 1 : 0