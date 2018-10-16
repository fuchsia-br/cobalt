// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config/id.h"

#include <string>

namespace cobalt {
namespace config {

const uint32_t kFnvPrime = 0x1000193;
const uint32_t kFnvOffsetBasis = 0x811c9dc5;

uint32_t IdFromName(const std::string &name) {
  uint32_t hash = kFnvOffsetBasis;
  for (size_t i = 0; i < name.size(); i++) {
    hash *= kFnvPrime;
    hash ^= name[i];
  }
  return hash;
}

}  // namespace config
}  // namespace cobalt
