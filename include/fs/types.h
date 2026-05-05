#pragma once

#include "fs/config.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum fs_file_type {
  FS_TYPE_REGULAR = 0,
  FS_TYPE_DIRECTORY,
  FS_TYPE_SYMLINK
} fs_file_type_t;

typedef struct fs_inode {
  uint32_t id;
  fs_file_type_t type;
  bool is_used;
  uint32_t link_count;
  uint32_t size;
  uint32_t cluster_count;
  uint32_t direct_clusters[FS_DIRECT_POINTERS];
  uint32_t single_indirect_cluster;
  uint32_t double_indirect_cluster;
  uint32_t triple_indirect_cluster;
} fs_inode_t;

typedef struct fs_dentry {
  uint32_t inode_id;
  uint16_t record_length;
  uint16_t name_length;
  char name[];
} fs_dentry_t;

static inline uint32_t
fs_dentry_calculate_record_length(const uint16_t name_length) {
  const uint32_t base_length =
    sizeof(fs_dentry_t) + (uint32_t)name_length + 1U;
  const uint32_t alignment_mask = 3U;
  return (base_length + alignment_mask) & ~alignment_mask;
}

static inline fs_dentry_t*
fs_dentry_create(const uint32_t inode_id, const char* name) {
  if (name == nullptr) {
    return nullptr;
  }

  const uint16_t name_length = (uint16_t)strlen(name);
  const uint32_t record_length =
    fs_dentry_calculate_record_length(name_length);

  fs_dentry_t* dentry_context = calloc(1, record_length);
  if (dentry_context == nullptr) {
    return nullptr;
  }

  dentry_context->inode_id = inode_id;
  dentry_context->record_length = record_length;
  dentry_context->name_length = name_length;
  memcpy(dentry_context->name, name, name_length + 1U);

  return dentry_context;
}

static inline void
fs_dentry_destroy(fs_dentry_t* dentry_context) {
  if (dentry_context != nullptr) {
    free(dentry_context);
  }
}

typedef struct fs_superblock {
  uint32_t magic;
  uint32_t total_clusters;
  uint32_t root_inode_id;
  uint32_t inode_table_start_cluster;
  uint32_t inode_table_size_clusters;
  uint32_t data_start_cluster;
} fs_superblock_t;