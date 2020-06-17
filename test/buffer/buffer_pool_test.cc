// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include <string>

#include "buffer.h"
#include "utils.h"

void release_pool_buffer(easymedia::BufferPool *pool) {
  int i = 100;

  while (i-- > 0) {
    auto mb1 = pool->GetBuffer();
    if (mb1) {
      LOG("T1: get msg: ptr:%p, fd:%d, size:%zu\n",
        mb1->GetPtr(), mb1->GetFD(), mb1->GetSize());
      easymedia::usleep(50000);
    } else {
      LOG("ERROR: T1: get msb failed!\n");
      pool->DumpInfo();
    }
  }
}

int main() {
  LOG_INIT();

  easymedia::BufferPool pool(10, 1024, easymedia::MediaBuffer::MemType::MEM_COMMON);;

  LOG("#001 Dump Info....\n");
  pool.DumpInfo();

  LOG("--> Alloc 1 buffer from buffer pool\n");
  auto mb0 = pool.GetBuffer();
  LOG("--> mb0: ptr:%p, fd:%d, size:%zu\n", mb0->GetPtr(), mb0->GetFD(), mb0->GetSize());

  LOG("#002 Dump Info....\n");
  pool.DumpInfo();

  LOG("--> reset mb0\n");
  mb0.reset();

  LOG("#003 Dump Info....\n");
  pool.DumpInfo();

  std::thread *thread = new std::thread(release_pool_buffer, &pool);

  int i = 100;
  std::list<std::shared_ptr<easymedia::MediaBuffer>> list;
  while (i-- > 0) {
    mb0 = pool.GetBuffer();
    if (mb0) {
      LOG("T0: get msg: ptr:%p, fd:%d, size:%zu\n",
        mb0->GetPtr(), mb0->GetFD(), mb0->GetSize());
      easymedia::usleep(50000);
    } else {
      LOG("ERROR: T0: get msb failed!\n");
      pool.DumpInfo();
    }

    list.push_back(mb0);
    if (list.size() >= 10) {
      LOG("--> List size:%zu, sleep 5s....\n", list.size());
      easymedia::usleep(5000000);
      int j = 0;
      while (list.size()) {
        LOG("--> (%d) Free 1 msg from list, sleep 5s...\n", j++);
        list.pop_front();
        easymedia::usleep(5000000);
      }
    }
  }

  thread->join();
  LOG("===== FINISH ====\n");
  return 0;
}

