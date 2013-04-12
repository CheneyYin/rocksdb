// Copyright 2008-present Facebook. All Rights Reserved.
#ifndef STORAGE_LEVELDB_INCLUDE_LDB_TOOL_H
#define STORAGE_LEVELDB_INCLUDE_LDB_TOOL_H
#include "leveldb/options.h"

namespace leveldb {

class LDBTool {
 public:
  void Run(int argc, char** argv, Options = Options());
};

} // namespace leveldb
#endif // STORAGE_LEVELDB_INCLUDE_LDB_TOOL_H
