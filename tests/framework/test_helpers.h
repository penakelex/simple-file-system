#pragma once

#include "fs/alloc/alloc.h"
#include "fs/logical/dir.h"
#include "fs/metadata/index.h"
#include "fs/space/bitmap.h"
#include "fs/storage/disk.h"
#include "fs/storage/format.h"
#include "fs/storage/superblock.h"
#include "fs/vfs/vfs.h"

#define TEST_DISK_PATH "test_disk.bin"
#define TEST_CLUSTER_COUNT 256U

typedef struct test_environment {
  fs_disk_t* disk_context;
  fs_bitmap_t* bitmap_context;
  fs_index_t* index_context;
  fs_alloc_context_t* alloc_context;
  fs_dir_context_t* dir_context;
  fs_vfs_context_t* vfs_context;
  uint32_t root_inode_id;
} test_environment_t;

void test_cleanup_disk_file(const char* file_path);
fs_status_t test_setup_full_environment(
  test_environment_t* environment);
fs_status_t
test_close_environment(test_environment_t* environment);
fs_status_t
test_reopen_environment(test_environment_t* environment);
fs_status_t test_teardown_full_environment(
  test_environment_t* environment);