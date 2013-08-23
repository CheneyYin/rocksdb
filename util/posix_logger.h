// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Logger implementation that can be shared by all environments
// where enough posix functionality is available.

#ifndef STORAGE_LEVELDB_UTIL_POSIX_LOGGER_H_
#define STORAGE_LEVELDB_UTIL_POSIX_LOGGER_H_

#include <algorithm>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef OS_LINUX
#include <linux/falloc.h>
#endif
#include "rocksdb/env.h"
#include <atomic>

namespace leveldb {

const int kDebugLogChunkSize = 128 * 1024;

class PosixLogger : public Logger {
 private:
  FILE* file_;
  uint64_t (*gettid_)();  // Return the thread id for the current thread
  std::atomic_size_t log_size_;
  int fd_;
 public:
  PosixLogger(FILE* f, uint64_t (*gettid)()) :
    file_(f), gettid_(gettid), log_size_(0), fd_(fileno(f)) { }
  virtual ~PosixLogger() {
    fclose(file_);
  }
  virtual void Logv(const char* format, va_list ap) {
    const uint64_t thread_id = (*gettid_)();

    // We try twice: the first time with a fixed-size stack allocated buffer,
    // and the second time with a much larger dynamically allocated buffer.
    char buffer[500];
    for (int iter = 0; iter < 2; iter++) {
      char* base;
      int bufsize;
      if (iter == 0) {
        bufsize = sizeof(buffer);
        base = buffer;
      } else {
        bufsize = 30000;
        base = new char[bufsize];
      }
      char* p = base;
      char* limit = base + bufsize;

      struct timeval now_tv;
      gettimeofday(&now_tv, nullptr);
      const time_t seconds = now_tv.tv_sec;
      struct tm t;
      localtime_r(&seconds, &t);
      p += snprintf(p, limit - p,
                    "%04d/%02d/%02d-%02d:%02d:%02d.%06d %llx ",
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec,
                    static_cast<int>(now_tv.tv_usec),
                    static_cast<long long unsigned int>(thread_id));

      // Print the message
      if (p < limit) {
        va_list backup_ap;
        va_copy(backup_ap, ap);
        p += vsnprintf(p, limit - p, format, backup_ap);
        va_end(backup_ap);
      }

      // Truncate to available space if necessary
      if (p >= limit) {
        if (iter == 0) {
          continue;       // Try again with larger buffer
        } else {
          p = limit - 1;
        }
      }

      // Add newline if necessary
      if (p == base || p[-1] != '\n') {
        *p++ = '\n';
      }

      assert(p <= limit);
      const size_t write_size = p - base;

#ifdef OS_LINUX
      // If this write would cross a boundary of kDebugLogChunkSize
      // space, pre-allocate more space to avoid overly large
      // allocations from filesystem allocsize options.
      const size_t log_size = log_size_;
      const int last_allocation_chunk =
        ((kDebugLogChunkSize - 1 + log_size) / kDebugLogChunkSize);
      const int desired_allocation_chunk =
        ((kDebugLogChunkSize - 1 + log_size + write_size) /
           kDebugLogChunkSize);
      if (last_allocation_chunk != desired_allocation_chunk) {
        fallocate(fd_, FALLOC_FL_KEEP_SIZE, 0,
                  desired_allocation_chunk * kDebugLogChunkSize);
      }
#endif

      size_t sz = fwrite(base, 1, write_size, file_);
      assert(sz == write_size);
      if (sz > 0) {
        fflush(file_);
        log_size_ += write_size;
      }
      if (base != buffer) {
        delete[] base;
      }
      break;
    }
  }
  size_t GetLogFileSize() const {
    return log_size_;
  }
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_POSIX_LOGGER_H_
