#include "framework/test_framework.h"
#include "framework/test_helpers.h"
#include "fs/logical/dir.h"

static void
list_entries_count_callback(const char* entry_name,
                            const uint32_t entry_inode_id,
                            const fs_file_type_t entry_type,
                            void* user_data) {
  (void)entry_name;
  (void)entry_inode_id;
  (void)entry_type;
  uint32_t* count = (uint32_t*)user_data;
  (*count)++;
}

static void test_dir_create_new_directory() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t new_dir_inode_id = 0;
  (void)fs_index_allocate_inode(environment.index_context,
                                FS_TYPE_DIRECTORY,
                                &new_dir_inode_id);

  const fs_status_t status =
    fs_dir_create_new(environment.dir_context,
                      new_dir_inode_id,
                      environment.root_inode_id);
  TEST_ASSERT_STATUS_OK(
    status, "create new directory should succeed");

  fs_inode_t dir_inode = {0};
  (void)fs_index_read_inode(environment.index_context,
                            new_dir_inode_id,
                            &dir_inode);
  TEST_ASSERT_EQUAL_INT(FS_TYPE_DIRECTORY,
                        dir_inode.type,
                        "type should be directory");
  TEST_ASSERT(dir_inode.size > 0U,
              "directory should have entries (. and ..)");

  (void)test_teardown_full_environment(&environment);
}

static void test_dir_insert_and_find_entry() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t file_inode_id = 0;
  (void)fs_index_allocate_inode(environment.index_context,
                                FS_TYPE_REGULAR,
                                &file_inode_id);

  fs_status_t status =
    fs_dir_insert_entry(environment.dir_context,
                        environment.root_inode_id,
                        "test_file.txt",
                        file_inode_id);
  TEST_ASSERT_STATUS_OK(status,
                        "insert entry should succeed");

  uint32_t found_inode_id = 0;
  status = fs_dir_find_entry(environment.dir_context,
                             environment.root_inode_id,
                             "test_file.txt",
                             &found_inode_id);
  TEST_ASSERT_STATUS_OK(status,
                        "find entry should succeed");
  TEST_ASSERT_EQUAL_UINT(file_inode_id,
                         found_inode_id,
                         "found inode should match");

  (void)test_teardown_full_environment(&environment);
}

static void test_dir_insert_duplicate_name() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t file_inode_id = 0;
  (void)fs_index_allocate_inode(environment.index_context,
                                FS_TYPE_REGULAR,
                                &file_inode_id);

  (void)fs_dir_insert_entry(environment.dir_context,
                            environment.root_inode_id,
                            "duplicate.txt",
                            file_inode_id);

  uint32_t another_inode_id = 0;
  (void)fs_index_allocate_inode(environment.index_context,
                                FS_TYPE_REGULAR,
                                &another_inode_id);

  const fs_status_t status =
    fs_dir_insert_entry(environment.dir_context,
                        environment.root_inode_id,
                        "duplicate.txt",
                        another_inode_id);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "duplicate name should fail");

  (void)test_teardown_full_environment(&environment);
}

static void test_dir_remove_entry() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t file_inode_id = 0;
  (void)fs_index_allocate_inode(environment.index_context,
                                FS_TYPE_REGULAR,
                                &file_inode_id);

  (void)fs_dir_insert_entry(environment.dir_context,
                            environment.root_inode_id,
                            "removable.txt",
                            file_inode_id);

  const fs_status_t remove_status =
    fs_dir_remove_entry(environment.dir_context,
                        environment.root_inode_id,
                        "removable.txt");
  TEST_ASSERT_STATUS_OK(remove_status,
                        "remove entry should succeed");

  uint32_t found_inode_id = 0;
  const fs_status_t find_status =
    fs_dir_find_entry(environment.dir_context,
                      environment.root_inode_id,
                      "removable.txt",
                      &found_inode_id);
  TEST_ASSERT_STATUS_EQUAL(
    FS_STATUS_ERROR_OUT_OF_BOUNDS,
    find_status,
    "removed entry should not be found");

  (void)test_teardown_full_environment(&environment);
}

static void test_dir_rename_entry() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t file_inode_id = 0;
  (void)fs_index_allocate_inode(environment.index_context,
                                FS_TYPE_REGULAR,
                                &file_inode_id);

  (void)fs_dir_insert_entry(environment.dir_context,
                            environment.root_inode_id,
                            "old_name.txt",
                            file_inode_id);

  const fs_status_t rename_status =
    fs_dir_rename_entry(environment.dir_context,
                        environment.root_inode_id,
                        "old_name.txt",
                        "new_name.txt");
  TEST_ASSERT_STATUS_OK(rename_status,
                        "rename should succeed");

  uint32_t found_inode_id = 0;
  fs_status_t status =
    fs_dir_find_entry(environment.dir_context,
                      environment.root_inode_id,
                      "new_name.txt",
                      &found_inode_id);
  TEST_ASSERT_STATUS_OK(status,
                        "find new name should succeed");
  TEST_ASSERT_EQUAL_UINT(file_inode_id,
                         found_inode_id,
                         "inode should match after rename");

  status = fs_dir_find_entry(environment.dir_context,
                             environment.root_inode_id,
                             "old_name.txt",
                             &found_inode_id);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_OUT_OF_BOUNDS,
                           status,
                           "old name should not exist");

  (void)test_teardown_full_environment(&environment);
}

static void test_dir_list_entries() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t entry_count = 0;

  for (uint32_t file_index = 0; file_index < 5U;
       ++file_index) {
    uint32_t file_inode_id = 0;
    (void)fs_index_allocate_inode(environment.index_context,
                                  FS_TYPE_REGULAR,
                                  &file_inode_id);

    char file_name[32];
    snprintf(file_name,
             sizeof(file_name),
             "file_%u.txt",
             file_index);
    (void)fs_dir_insert_entry(environment.dir_context,
                              environment.root_inode_id,
                              file_name,
                              file_inode_id);
  }

  const fs_status_t status =
    fs_dir_list_entries(environment.dir_context,
                        environment.root_inode_id,
                        list_entries_count_callback,
                        &entry_count);
  TEST_ASSERT_STATUS_OK(status,
                        "list entries should succeed");
  TEST_ASSERT(entry_count >= 5U,
              "should have at least 5 entries");

  (void)test_teardown_full_environment(&environment);
}

static void test_dir_is_empty() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t new_dir_inode_id = 0;
  (void)fs_index_allocate_inode(environment.index_context,
                                FS_TYPE_DIRECTORY,
                                &new_dir_inode_id);
  (void)fs_dir_create_new(environment.dir_context,
                          new_dir_inode_id,
                          environment.root_inode_id);

  bool is_empty = false;
  fs_status_t status = fs_dir_is_empty(
    environment.dir_context, new_dir_inode_id, &is_empty);
  TEST_ASSERT_STATUS_OK(status, "is_empty should succeed");
  TEST_ASSERT(is_empty, "new directory should be empty");

  uint32_t file_inode_id = 0;
  (void)fs_index_allocate_inode(environment.index_context,
                                FS_TYPE_REGULAR,
                                &file_inode_id);
  (void)fs_dir_insert_entry(environment.dir_context,
                            new_dir_inode_id,
                            "file.txt",
                            file_inode_id);

  status = fs_dir_is_empty(
    environment.dir_context, new_dir_inode_id, &is_empty);
  TEST_ASSERT_STATUS_OK(status, "is_empty should succeed");
  TEST_ASSERT(!is_empty,
              "directory with file should not be empty");

  (void)test_teardown_full_environment(&environment);
}

static void test_dir_invalid_names() {
  test_environment_t environment = {0};
  (void)test_setup_full_environment(&environment);

  uint32_t file_inode_id = 0;
  (void)fs_index_allocate_inode(environment.index_context,
                                FS_TYPE_REGULAR,
                                &file_inode_id);

  fs_status_t status =
    fs_dir_insert_entry(environment.dir_context,
                        environment.root_inode_id,
                        ".",
                        file_inode_id);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "dot name should fail");

  status = fs_dir_insert_entry(environment.dir_context,
                               environment.root_inode_id,
                               "..",
                               file_inode_id);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "dot-dot name should fail");

  status = fs_dir_insert_entry(environment.dir_context,
                               environment.root_inode_id,
                               "path/name.txt",
                               file_inode_id);
  TEST_ASSERT_STATUS_EQUAL(FS_STATUS_ERROR_INVALID_ARGUMENT,
                           status,
                           "name with slash should fail");

  (void)test_teardown_full_environment(&environment);
}

int main() {
  TEST_SUITE_BEGIN("Directory Layer");
  TEST_RUN(test_dir_create_new_directory);
  TEST_RUN(test_dir_insert_and_find_entry);
  TEST_RUN(test_dir_insert_duplicate_name);
  TEST_RUN(test_dir_remove_entry);
  TEST_RUN(test_dir_rename_entry);
  TEST_RUN(test_dir_list_entries);
  TEST_RUN(test_dir_is_empty);
  TEST_RUN(test_dir_invalid_names);
  TEST_SUITE_END();
  TEST_EXIT();
}