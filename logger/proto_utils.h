// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_LOGGER_PROTO_UTILS_H_
#define COBALT_LOGGER_PROTO_UTILS_H_

#include <memory>
#include <string>

#include "./observation2.pb.h"
#include "config/metric_definition.pb.h"
#include "config/report_definition.pb.h"

namespace cobalt {
namespace logger {

// A HistogramPtr provides a moveable way of passing the buckets of a Histogram.
typedef std::unique_ptr<google::protobuf::RepeatedPtrField<HistogramBucket>>
    HistogramPtr;


// Miscellaneous utilities for working with Cobalt protos.
class ProtoUtils {
 public:
  // Returns the name of the given MetricType enum.
  static std::string EnumName(MetricDefinition::MetricType metric_type);
};

}  // namespace logger
}  // namespace cobalt

#endif  // COBALT_LOGGER_PROTO_UTILS_H_
