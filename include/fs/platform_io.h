#pragma once
#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

[[maybe_unused]] static inline int
fs_file_seek(FILE* stream,
             const int64_t byte_offset,
             const int origin) {
#ifdef _WIN32
  return _fseeki64(stream, byte_offset, origin);
#else
  return fseeko(stream, byte_offset, origin);
#endif
}

[[maybe_unused]] static inline int64_t
fs_file_tell(FILE* stream) {
#ifdef _WIN32
  return _ftelli64(stream);
#else
  return (int64_t)ftello(stream);
#endif
}

[[maybe_unused]] static inline int
fs_file_sync(FILE* stream) {
  if (fflush(stream) != 0) {
    return -1;
  }

#ifdef _WIN32
  const int file_descriptor = _fileno(stream);
  
  if (file_descriptor == -1) {
    return -1;
  }

  return _commit(file_descriptor);
#else
  const int file_descriptor = fileno(stream);
  
  if (file_descriptor == -1) {
    return -1;
  }
  
  return fsync(file_descriptor);
#endif
}