// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "logger/internal_metrics.h"

#include <memory>
#include <string>
#include <utility>

#include "./logging.h"

namespace cobalt {
namespace logger {

InternalMetricsImpl::InternalMetricsImpl(LoggerInterface* logger)
    : logger_(logger) {
  CHECK(logger_);
}

void InternalMetricsImpl::LoggerCalled(LoggerCallsMadeEventCode event_code) {
  logger_->LogEvent(kLoggerCallsMadeMetricId,
                    static_cast<uint32_t>(event_code));
}

}  // namespace logger
}  // namespace cobalt
