#include "fs/alloc.h"
#include <stdlib.h>
#include <string.h>

struct fs_alloc_context {
  fs_disk_t* disk_context;
  fs_bitmap_t* bitmap_context;
  fs_index_t* index_context;
};

[[nodiscard]] static fs_status_t
fs_alloc_allocate_and_zero_cluster(
  fs_alloc_context_t* alloc_context,
  uint32_t* output_cluster_index) {
  uint32_t new_cluster_index = 0;
  fs_status_t status = fs_bitmap_find_free_cluster(
    alloc_context->bitmap_context, &new_cluster_index);

  if (status != FS_STATUS_OK) {
    return status;
  }

  status = fs_bitmap_mark_cluster_used(
    alloc_context->bitmap_context, new_cluster_index);

  if (status != FS_STATUS_OK) {
    return status;
  }

  uint8_t zero_buffer[FS_CLUSTER_SIZE] = {0};
  status =
    fs_disk_write_cluster(alloc_context->disk_context,
                          new_cluster_index,
                          zero_buffer);

  if (status != FS_STATUS_OK) {
    (void)fs_bitmap_mark_cluster_free(
      alloc_context->bitmap_context, new_cluster_index);
    return status;
  }

  *output_cluster_index = new_cluster_index;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t fs_alloc_create_context(
  fs_alloc_context_t** output_alloc_context,
  fs_disk_t* disk_context,
  fs_bitmap_t* bitmap_context,
  fs_index_t* index_context) {
  if (output_alloc_context == nullptr
      || disk_context == nullptr
      || bitmap_context == nullptr
      || index_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  fs_alloc_context_t* alloc_context =
    calloc(1, sizeof(fs_alloc_context_t));

  if (alloc_context == nullptr) {
    return FS_STATUS_ERROR_MEMORY_ALLOCATION;
  }

  alloc_context->disk_context = disk_context;
  alloc_context->bitmap_context = bitmap_context;
  alloc_context->index_context = index_context;
  *output_alloc_context = alloc_context;

  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t fs_alloc_destroy_context(
  fs_alloc_context_t* alloc_context) {
  if (alloc_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  free(alloc_context);
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t fs_alloc_resolve_cluster(
  fs_alloc_context_t* alloc_context,
  fs_inode_t* inode_context,
  uint32_t logical_cluster_index,
  bool allocate_on_missing,
  uint32_t* output_physical_cluster_index) {
  if (alloc_context == nullptr || inode_context == nullptr
      || output_physical_cluster_index == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  const uint32_t pointers_per_cluster =
    FS_CLUSTER_SIZE / sizeof(uint32_t);
  uint32_t target_cluster_index = 0;
  fs_status_t status = FS_STATUS_OK;

  if (logical_cluster_index < FS_DIRECT_POINTERS) {
    target_cluster_index =
      inode_context->direct_clusters[logical_cluster_index];

    if (target_cluster_index == 0 && allocate_on_missing) {
      status = fs_alloc_allocate_and_zero_cluster(
        alloc_context, &target_cluster_index);

      if (status != FS_STATUS_OK) {
        return status;
      }

      inode_context
        ->direct_clusters[logical_cluster_index] =
        target_cluster_index;
    }
  } else if (logical_cluster_index
             < FS_DIRECT_POINTERS + pointers_per_cluster) {
    uint32_t single_indirect =
      inode_context->single_indirect_cluster;

    if (single_indirect == 0 && allocate_on_missing) {
      status = fs_alloc_allocate_and_zero_cluster(
        alloc_context, &single_indirect);

      if (status != FS_STATUS_OK) {
        return status;
      }

      inode_context->single_indirect_cluster =
        single_indirect;
    }

    if (single_indirect == 0) {
      return FS_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    uint32_t indirect_pointers[pointers_per_cluster];
    status =
      fs_disk_read_cluster(alloc_context->disk_context,
                           single_indirect,
                           indirect_pointers);

    if (status != FS_STATUS_OK) {
      return status;
    }

    const uint32_t offset =
      logical_cluster_index - FS_DIRECT_POINTERS;
    target_cluster_index = indirect_pointers[offset];

    if (target_cluster_index == 0 && allocate_on_missing) {
      status = fs_alloc_allocate_and_zero_cluster(
        alloc_context, &target_cluster_index);

      if (status != FS_STATUS_OK) {
        return status;
      }

      indirect_pointers[offset] = target_cluster_index;
      status =
        fs_disk_write_cluster(alloc_context->disk_context,
                              single_indirect,
                              indirect_pointers);

      if (status != FS_STATUS_OK) {
        return status;
      }
    }
  } else if (logical_cluster_index
             < FS_DIRECT_POINTERS + pointers_per_cluster
                 + (pointers_per_cluster
                    * pointers_per_cluster)) {
    uint32_t double_indirect =
      inode_context->double_indirect_cluster;

    if (double_indirect == 0 && allocate_on_missing) {
      status = fs_alloc_allocate_and_zero_cluster(
        alloc_context, &double_indirect);

      if (status != FS_STATUS_OK) {
        return status;
      }

      inode_context->double_indirect_cluster =
        double_indirect;
    }

    if (double_indirect == 0) {
      return FS_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    uint32_t s_pointers[pointers_per_cluster];
    status =
      fs_disk_read_cluster(alloc_context->disk_context,
                           double_indirect,
                           s_pointers);

    if (status != FS_STATUS_OK) {
      return status;
    }

    const uint32_t remaining = logical_cluster_index
                               - FS_DIRECT_POINTERS
                               - pointers_per_cluster;
    const uint32_t outer_offset =
      remaining / pointers_per_cluster;
    const uint32_t inner_offset =
      remaining % pointers_per_cluster;

    uint32_t single_indirect = s_pointers[outer_offset];

    if (single_indirect == 0 && allocate_on_missing) {
      status = fs_alloc_allocate_and_zero_cluster(
        alloc_context, &single_indirect);

      if (status != FS_STATUS_OK) {
        return status;
      }

      s_pointers[outer_offset] = single_indirect;
      status =
        fs_disk_write_cluster(alloc_context->disk_context,
                              double_indirect,
                              s_pointers);

      if (status != FS_STATUS_OK) {
        return status;
      }
    }

    if (single_indirect == 0) {
      return FS_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    uint32_t d_pointers[pointers_per_cluster];
    status =
      fs_disk_read_cluster(alloc_context->disk_context,
                           single_indirect,
                           d_pointers);

    if (status != FS_STATUS_OK) {
      return status;
    }

    target_cluster_index = d_pointers[inner_offset];

    if (target_cluster_index == 0 && allocate_on_missing) {
      status = fs_alloc_allocate_and_zero_cluster(
        alloc_context, &target_cluster_index);

      if (status != FS_STATUS_OK) {
        return status;
      }

      d_pointers[inner_offset] = target_cluster_index;
      status =
        fs_disk_write_cluster(alloc_context->disk_context,
                              single_indirect,
                              d_pointers);

      if (status != FS_STATUS_OK) {
        return status;
      }
    }
  } else {
    uint32_t triple_indirect =
      inode_context->triple_indirect_cluster;

    if (triple_indirect == 0 && allocate_on_missing) {
      status = fs_alloc_allocate_and_zero_cluster(
        alloc_context, &triple_indirect);

      if (status != FS_STATUS_OK) {
        return status;
      }

      inode_context->triple_indirect_cluster =
        triple_indirect;
    }

    if (triple_indirect == 0) {
      return FS_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    uint32_t t_pointers[pointers_per_cluster];
    status =
      fs_disk_read_cluster(alloc_context->disk_context,
                           triple_indirect,
                           t_pointers);

    if (status != FS_STATUS_OK) {
      return status;
    }

    const uint32_t remaining =
      logical_cluster_index - FS_DIRECT_POINTERS
      - pointers_per_cluster
      - (pointers_per_cluster * pointers_per_cluster);
    const uint32_t outer_offset =
      remaining
      / (pointers_per_cluster * pointers_per_cluster);
    const uint32_t mid_remaining =
      remaining
      % (pointers_per_cluster * pointers_per_cluster);
    const uint32_t mid_offset =
      mid_remaining / pointers_per_cluster;
    const uint32_t inner_offset =
      mid_remaining % pointers_per_cluster;

    uint32_t double_indirect = t_pointers[outer_offset];

    if (double_indirect == 0 && allocate_on_missing) {
      status = fs_alloc_allocate_and_zero_cluster(
        alloc_context, &double_indirect);

      if (status != FS_STATUS_OK) {
        return status;
      }

      t_pointers[outer_offset] = double_indirect;
      status =
        fs_disk_write_cluster(alloc_context->disk_context,
                              triple_indirect,
                              t_pointers);

      if (status != FS_STATUS_OK) {
        return status;
      }
    }

    if (double_indirect == 0) {
      return FS_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    uint32_t s_pointers[pointers_per_cluster];
    status =
      fs_disk_read_cluster(alloc_context->disk_context,
                           double_indirect,
                           s_pointers);

    if (status != FS_STATUS_OK) {
      return status;
    }

    uint32_t single_indirect = s_pointers[mid_offset];

    if (single_indirect == 0 && allocate_on_missing) {
      status = fs_alloc_allocate_and_zero_cluster(
        alloc_context, &single_indirect);

      if (status != FS_STATUS_OK) {
        return status;
      }

      s_pointers[mid_offset] = single_indirect;
      status =
        fs_disk_write_cluster(alloc_context->disk_context,
                              double_indirect,
                              s_pointers);

      if (status != FS_STATUS_OK) {
        return status;
      }
    }

    if (single_indirect == 0) {
      return FS_STATUS_ERROR_OUT_OF_BOUNDS;
    }

    uint32_t d_pointers[pointers_per_cluster];
    status =
      fs_disk_read_cluster(alloc_context->disk_context,
                           single_indirect,
                           d_pointers);

    if (status != FS_STATUS_OK) {
      return status;
    }

    target_cluster_index = d_pointers[inner_offset];

    if (target_cluster_index == 0 && allocate_on_missing) {
      status = fs_alloc_allocate_and_zero_cluster(
        alloc_context, &target_cluster_index);

      if (status != FS_STATUS_OK) {
        return status;
      }

      d_pointers[inner_offset] = target_cluster_index;
      status =
        fs_disk_write_cluster(alloc_context->disk_context,
                              single_indirect,
                              d_pointers);

      if (status != FS_STATUS_OK) {
        return status;
      }
    }
  }

  if (target_cluster_index == 0) {
    return FS_STATUS_ERROR_OUT_OF_BOUNDS;
  }

  *output_physical_cluster_index = target_cluster_index;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_alloc_read_data(fs_alloc_context_t* alloc_context,
                   const fs_inode_t* inode_context,
                   size_t byte_offset,
                   void* destination_buffer,
                   size_t bytes_to_read,
                   size_t* bytes_actually_read) {
  if (alloc_context == nullptr || inode_context == nullptr
      || destination_buffer == nullptr
      || bytes_actually_read == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  if (byte_offset >= inode_context->size) {
    *bytes_actually_read = 0;
    return FS_STATUS_OK;
  }

  const size_t bytes_available =
    inode_context->size - byte_offset;
  const size_t bytes_to_copy =
    bytes_to_read > bytes_available ? bytes_available
                                    : bytes_to_read;
  size_t total_bytes_read = 0;

  while (total_bytes_read < bytes_to_copy) {
    const uint32_t logical_cluster_index =
      (uint32_t)((byte_offset + total_bytes_read)
                 / FS_CLUSTER_SIZE);
    const size_t offset_in_cluster =
      (byte_offset + total_bytes_read) % FS_CLUSTER_SIZE;
    const size_t bytes_in_cluster =
      FS_CLUSTER_SIZE - offset_in_cluster;
    const size_t chunk_size =
      bytes_to_copy - total_bytes_read > bytes_in_cluster
        ? bytes_in_cluster
        : bytes_to_copy - total_bytes_read;

    uint32_t physical_cluster_index = 0;
    fs_status_t status =
      fs_alloc_resolve_cluster(alloc_context,
                               (fs_inode_t*)inode_context,
                               logical_cluster_index,
                               false,
                               &physical_cluster_index);

    if (status != FS_STATUS_OK) {
      return status;
    }

    uint8_t cluster_buffer[FS_CLUSTER_SIZE] = {0};
    status =
      fs_disk_read_cluster(alloc_context->disk_context,
                           physical_cluster_index,
                           cluster_buffer);

    if (status != FS_STATUS_OK) {
      return status;
    }

    memcpy((uint8_t*)destination_buffer + total_bytes_read,
           cluster_buffer + offset_in_cluster,
           chunk_size);

    total_bytes_read += chunk_size;
  }

  *bytes_actually_read = total_bytes_read;
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_alloc_write_data(fs_alloc_context_t* alloc_context,
                    fs_inode_t* inode_context,
                    size_t byte_offset,
                    const void* source_buffer,
                    size_t bytes_to_write,
                    size_t* bytes_actually_written) {
  if (alloc_context == nullptr || inode_context == nullptr
      || source_buffer == nullptr
      || bytes_actually_written == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  size_t total_bytes_written = 0;

  while (total_bytes_written < bytes_to_write) {
    const uint32_t logical_cluster_index =
      (uint32_t)((byte_offset + total_bytes_written)
                 / FS_CLUSTER_SIZE);
    const size_t offset_in_cluster =
      (byte_offset + total_bytes_written) % FS_CLUSTER_SIZE;
    const size_t bytes_in_cluster =
      FS_CLUSTER_SIZE - offset_in_cluster;
    const size_t chunk_size =
      bytes_to_write - total_bytes_written
          > bytes_in_cluster
        ? bytes_in_cluster
        : bytes_to_write - total_bytes_written;

    uint32_t physical_cluster_index = 0;
    fs_status_t status =
      fs_alloc_resolve_cluster(alloc_context,
                               inode_context,
                               logical_cluster_index,
                               true,
                               &physical_cluster_index);

    if (status != FS_STATUS_OK) {
      return status;
    }

    uint8_t cluster_buffer[FS_CLUSTER_SIZE] = {0};

    if (chunk_size < FS_CLUSTER_SIZE
        && offset_in_cluster > 0) {
      status =
        fs_disk_read_cluster(alloc_context->disk_context,
                             physical_cluster_index,
                             cluster_buffer);

      if (status != FS_STATUS_OK)
        return status;
    }

    memcpy(
      cluster_buffer + offset_in_cluster,
      (const uint8_t*)source_buffer + total_bytes_written,
      chunk_size);

    status =
      fs_disk_write_cluster(alloc_context->disk_context,
                            physical_cluster_index,
                            cluster_buffer);

    if (status != FS_STATUS_OK) {
      return status;
    }

    total_bytes_written += chunk_size;

    if (byte_offset + total_bytes_written
        > inode_context->size) {
      inode_context->size =
        (uint32_t)(byte_offset + total_bytes_written);
    }
  }

  *bytes_actually_written = total_bytes_written;
  inode_context->cluster_count =
    (uint32_t)((inode_context->size + FS_CLUSTER_SIZE - 1)
               / FS_CLUSTER_SIZE);
  return FS_STATUS_OK;
}

[[nodiscard]] fs_status_t
fs_alloc_truncate_file(fs_alloc_context_t* alloc_context,
                       fs_inode_t* inode_context) {
  if (alloc_context == nullptr
      || inode_context == nullptr) {
    return FS_STATUS_ERROR_INVALID_ARGUMENT;
  }

  inode_context->size = 0;
  inode_context->cluster_count = 0;

  for (uint32_t i = 0; i < FS_DIRECT_POINTERS; ++i) {
    if (inode_context->direct_clusters[i] != 0) {
      (void)fs_bitmap_mark_cluster_free(
        alloc_context->bitmap_context,
        inode_context->direct_clusters[i]);
      inode_context->direct_clusters[i] = 0;
    }
  }

  if (inode_context->single_indirect_cluster != 0) {
    (void)fs_bitmap_mark_cluster_free(
      alloc_context->bitmap_context,
      inode_context->single_indirect_cluster);
    inode_context->single_indirect_cluster = 0;
  }

  if (inode_context->double_indirect_cluster != 0) {
    (void)fs_bitmap_mark_cluster_free(
      alloc_context->bitmap_context,
      inode_context->double_indirect_cluster);
    inode_context->double_indirect_cluster = 0;
  }

  if (inode_context->triple_indirect_cluster != 0) {
    (void)fs_bitmap_mark_cluster_free(
      alloc_context->bitmap_context,
      inode_context->triple_indirect_cluster);
    inode_context->triple_indirect_cluster = 0;
  }

  return fs_index_write_inode(alloc_context->index_context,
                              inode_context);
}