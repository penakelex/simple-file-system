#include "framework/test_framework.h"
#include "framework/test_helpers.h"
#include "fs/space/bitmap.h"

static void test_bitmap_create_destroy() {
  fs_bitmap_t* bitmap_context = nullptr;
  const fs_status_t status =
    fs_bitmap_create(&bitmap_context, 1024U);
  TEST_ASSERT_STATUS_OK(status,
                        "bitmap create should succeed");
  TEST_ASSERT(bitmap_context != nullptr,
              "bitmap should not be null");

  const uint32_t total_clusters =
    fs_bitmap_get_total_cluster_count(bitmap_context);
  TEST_ASSERT_EQUAL_UINT(
    1024U, total_clusters, "total clusters should match");

  const size_t byte_length =
    fs_bitmap_get_byte_length(bitmap_context);
  TEST_ASSERT_EQUAL_SIZE(
    128U, byte_length, "byte length should be 1024/8");

  const fs_status_t destroy_status =
    fs_bitmap_destroy(bitmap_context);
  TEST_ASSERT_STATUS_OK(destroy_status,
                        "bitmap destroy should succeed");
}

static void test_bitmap_create_invalid() {
  fs_bitmap_t* bitmap_context = nullptr;

  fs_status_t status = fs_bitmap_create(nullptr, 1024U);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "null output should fail");

  status = fs_bitmap_create(&bitmap_context, 0U);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "zero clusters should fail");
}

static void test_bitmap_find_free_sequential() {
  fs_bitmap_t* bitmap_context = nullptr;
  (void)fs_bitmap_create(&bitmap_context, 16U);

  for (uint32_t expected_cluster = 0U;
       expected_cluster < 16U;
       ++expected_cluster) {
    uint32_t free_cluster = UINT32_MAX;
    const fs_status_t status = fs_bitmap_find_free_cluster(
      bitmap_context, &free_cluster);
    TEST_ASSERT_STATUS_OK(status,
                          "find free should succeed");
    TEST_ASSERT_EQUAL_UINT(
      expected_cluster,
      free_cluster,
      "free cluster should match expected");

    (void)fs_bitmap_mark_cluster_used(bitmap_context,
                                      free_cluster);
  }

  uint32_t free_cluster = UINT32_MAX;
  const fs_status_t status = fs_bitmap_find_free_cluster(
    bitmap_context, &free_cluster);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_OUT_OF_BOUNDS,
                           status,
                           "no free clusters should fail");

  (void)fs_bitmap_destroy(bitmap_context);
}

static void test_bitmap_mark_and_free() {
  fs_bitmap_t* bitmap_context = nullptr;
  (void)fs_bitmap_create(&bitmap_context, 32U);

  (void)fs_bitmap_mark_cluster_used(bitmap_context, 5U);
  (void)fs_bitmap_mark_cluster_used(bitmap_context, 10U);
  (void)fs_bitmap_mark_cluster_used(bitmap_context, 20U);

  uint32_t free_cluster = UINT32_MAX;
  (void)fs_bitmap_find_free_cluster(bitmap_context,
                                    &free_cluster);
  TEST_ASSERT_EQUAL_UINT(
    0U, free_cluster, "first free should be 0");

  (void)fs_bitmap_mark_cluster_used(bitmap_context, 0U);
  (void)fs_bitmap_find_free_cluster(bitmap_context,
                                    &free_cluster);
  TEST_ASSERT_EQUAL_UINT(
    1U, free_cluster, "next free should be 1");

  const fs_status_t free_status =
    fs_bitmap_mark_cluster_free(bitmap_context, 5U);
  TEST_ASSERT_STATUS_OK(free_status,
                        "mark free should succeed");

  (void)fs_bitmap_mark_cluster_used(bitmap_context, 1U);
  (void)fs_bitmap_mark_cluster_used(bitmap_context, 2U);
  (void)fs_bitmap_mark_cluster_used(bitmap_context, 3U);
  (void)fs_bitmap_mark_cluster_used(bitmap_context, 4U);

  (void)fs_bitmap_find_free_cluster(bitmap_context,
                                    &free_cluster);
  TEST_ASSERT_EQUAL_UINT(
    5U,
    free_cluster,
    "freed cluster 5 should be available");

  (void)fs_bitmap_destroy(bitmap_context);
}

static void test_bitmap_boundary_conditions() {
  fs_bitmap_t* bitmap_context = nullptr;
  (void)fs_bitmap_create(&bitmap_context, 8U);

  for (uint32_t cluster_index = 0U; cluster_index < 8U;
       ++cluster_index) {
    (void)fs_bitmap_mark_cluster_used(bitmap_context,
                                      cluster_index);
  }

  uint32_t free_cluster = UINT32_MAX;
  fs_status_t status = fs_bitmap_find_free_cluster(
    bitmap_context, &free_cluster);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_OUT_OF_BOUNDS,
                           status,
                           "all clusters used should fail");

  (void)fs_bitmap_mark_cluster_free(bitmap_context, 7U);
  status = fs_bitmap_find_free_cluster(bitmap_context,
                                       &free_cluster);
  TEST_ASSERT_STATUS_OK(
    status, "find free should succeed after freeing last");
  TEST_ASSERT_EQUAL_UINT(
    7U, free_cluster, "last cluster should be free");

  (void)fs_bitmap_destroy(bitmap_context);
}

static void test_bitmap_out_of_bounds() {
  fs_bitmap_t* bitmap_context = nullptr;
  (void)fs_bitmap_create(&bitmap_context, 16U);

  fs_status_t status =
    fs_bitmap_mark_cluster_used(bitmap_context, 16U);
  TEST_ASSERT_STATUS_EQUAL(
    FS_STATUS_ERROR_OUT_OF_BOUNDS,
    status,
    "mark out of bounds should fail");

  status =
    fs_bitmap_mark_cluster_free(bitmap_context, 100U);
  TEST_ASSERT_STATUS_EQUAL(
    FS_STATUS_ERROR_OUT_OF_BOUNDS,
    status,
    "free out of bounds should fail");

  (void)fs_bitmap_destroy(bitmap_context);
}

static void test_bitmap_serialize_deserialize() {
  fs_disk_t* disk_context = nullptr;
  test_cleanup_disk_file(TEST_DISK_PATH);

  (void)fs_disk_create_or_open(
    &disk_context, TEST_DISK_PATH, 64U, false);

  fs_bitmap_t* original_bitmap = nullptr;
  (void)fs_bitmap_create(&original_bitmap, 64U);

  (void)fs_bitmap_mark_cluster_used(original_bitmap, 0U);
  (void)fs_bitmap_mark_cluster_used(original_bitmap, 5U);
  (void)fs_bitmap_mark_cluster_used(original_bitmap, 31U);
  (void)fs_bitmap_mark_cluster_used(original_bitmap, 32U);
  (void)fs_bitmap_mark_cluster_used(original_bitmap, 63U);

  fs_status_t status = fs_bitmap_serialize_to_disk(
    original_bitmap, disk_context);
  TEST_ASSERT_STATUS_OK(status, "serialize should succeed");

  fs_bitmap_t* loaded_bitmap = nullptr;
  (void)fs_bitmap_create(&loaded_bitmap, 64U);

  status = fs_bitmap_deserialize_from_disk(loaded_bitmap,
                                           disk_context);
  TEST_ASSERT_STATUS_OK(status,
                        "deserialize should succeed");

  uint32_t free_cluster = UINT32_MAX;
  (void)fs_bitmap_find_free_cluster(loaded_bitmap,
                                    &free_cluster);
  TEST_ASSERT_EQUAL_UINT(
    1U, free_cluster, "cluster 0 should be used");

  (void)fs_bitmap_mark_cluster_used(loaded_bitmap, 1U);
  (void)fs_bitmap_mark_cluster_used(loaded_bitmap, 2U);
  (void)fs_bitmap_mark_cluster_used(loaded_bitmap, 3U);
  (void)fs_bitmap_mark_cluster_used(loaded_bitmap, 4U);
  (void)fs_bitmap_find_free_cluster(loaded_bitmap,
                                    &free_cluster);
  TEST_ASSERT_EQUAL_UINT(
    6U, free_cluster, "cluster 5 should be used");

  (void)fs_bitmap_destroy(original_bitmap);
  (void)fs_bitmap_destroy(loaded_bitmap);
  (void)fs_disk_close(disk_context);
  test_cleanup_disk_file(TEST_DISK_PATH);
}

static void test_bitmap_null_arguments() {
  fs_bitmap_t* bitmap_context = nullptr;

  fs_status_t status = fs_bitmap_destroy(nullptr);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "destroy null should fail");

  status = fs_bitmap_find_free_cluster(nullptr, nullptr);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "find free null should fail");

  (void)fs_bitmap_create(&bitmap_context, 16U);
  status =
    fs_bitmap_find_free_cluster(bitmap_context, nullptr);
  TEST_ASSERT_STATUS_EQUAL(
    FS_STATUS_ERROR_INVALID_ARGUMENT,
    status,
    "find free null output should fail");

  (void)fs_bitmap_destroy(bitmap_context);
}

int main() {
  TEST_SUITE_BEGIN("Bitmap Layer");
  TEST_RUN(test_bitmap_create_destroy);
  TEST_RUN(test_bitmap_create_invalid);
  TEST_RUN(test_bitmap_find_free_sequential);
  TEST_RUN(test_bitmap_mark_and_free);
  TEST_RUN(test_bitmap_boundary_conditions);
  TEST_RUN(test_bitmap_out_of_bounds);
  TEST_RUN(test_bitmap_serialize_deserialize);
  TEST_RUN(test_bitmap_null_arguments);
  TEST_SUITE_END();
  TEST_EXIT();
}