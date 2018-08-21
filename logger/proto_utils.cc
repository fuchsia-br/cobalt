// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "logger/proto_utils.h"

namespace cobalt {
namespace logger {

std::string ProtoUtils::EnumName(MetricDefinition::MetricType metric_type) {
  switch (metric_type) {
    case MetricDefinition::EVENT_OCCURRED:
      return "EVENT_OCCURRED";

    case MetricDefinition::EVENT_COUNT:
      return "EVENT_COUNT";

    case MetricDefinition::ELAPSED_TIME:
      return "ELAPSED_TIME";

    case MetricDefinition::FRAME_RATE:
      return "FRAME_RATE";

    case MetricDefinition::MEMORY_USAGE:
      return "MEMORY_USAGE";

    case MetricDefinition::INT_HISTOGRAM:
      return "INT_HISTOGRAM";

    case MetricDefinition::STRING_USED:
      return "STRING_USED";

    case MetricDefinition::CUSTOM:
      return "CUSTOM";

    default:
      return "UNKNOWN";
  }
}

}  // namespace logger
}  // namespace cobalt
