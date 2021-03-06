// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_ENCODER_FAKE_SYSTEM_DATA_H_
#define COBALT_ENCODER_FAKE_SYSTEM_DATA_H_

#include <vector>

#include "encoder/system_data.h"

namespace cobalt {
namespace encoder {

// Mock of the SystemDataInterface. Used for testing.
class FakeSystemData : public SystemDataInterface {
 public:
  FakeSystemData() {
    system_profile_.set_os(SystemProfile::FUCHSIA);
    system_profile_.set_arch(SystemProfile::ARM_64);
    system_profile_.set_board_name("Testing Board");
    system_profile_.set_product_name("Testing Product");
  }

  const SystemProfile& system_profile() const override {
    return system_profile_;
  };

  const std::vector<Experiment>& experiments() const override {
    return experiments_;
  };

 private:
  SystemProfile system_profile_;
  std::vector<Experiment> experiments_;
};

}  // namespace encoder
}  // namespace cobalt

#endif  // COBALT_ENCODER_FAKE_SYSTEM_DATA_H_
