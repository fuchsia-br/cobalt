// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_LOGGER_LOGGER_H_
#define COBALT_LOGGER_LOGGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "./observation2.pb.h"
#include "encoder/observation_store.h"
#include "encoder/observation_store_update_recipient.h"
#include "logger/encoder.h"
#include "logger/logger_interface.h"
#include "logger/project_context.h"
#include "logger/status.h"
#include "util/clock.h"
#include "util/encrypted_message_util.h"

namespace cobalt {
namespace logger {

// Concrete implementation of LogerInterface.
//
// After constructing a Logger use the Log*() methods to Log Events to Cobalt.
//
// There should be an instance of Logger for each client-side Project.
// On a Fuchsia system instances of Logger are created by the Cobalt FIDL
// service for each FIDL connection from a client project.
class Logger : public LoggerInterface {
 public:
  // Constructor
  //
  // |encoder| The system's singleton instance of Encoder. This must remain
  // valid as long as the Logger is being used. The Logger uses this to
  // encode immediate Observations.
  //
  // |observation_store| A writer interface to the system's singleton instance
  // of Observation Store. This must remain valid as long as the Logger is
  // being used. The Logger uses this to write immediate Observations.
  //
  // |update_recipient| The Logger uses this to notify the update recipient
  // when an immediate Observation has been added to the Observation Store.
  // This must remain valid as long as the Logger is being used.
  //
  // |observation_encrypter| This is used to encrypt immediate Observations
  // to the public key of Cobalt's Analyzer prior to writing them into the
  // Observation Store. This must remain valid as long as the Logger is being
  // used.
  Logger(const Encoder* encoder,
         encoder::ObservationStoreWriterInterface* observation_store,
         encoder::ObservationStoreUpdateRecipient* update_recipient,
         const util::EncryptedMessageMaker* observation_encrypter,
         const ProjectContext* project);

  virtual ~Logger() = default;

  Status LogEvent(uint32_t metric_id, uint32_t event_type_index) override;

  Status LogEventCount(uint32_t metric_id, uint32_t event_type_index,
                       const std::string& component,
                       int64_t period_duration_micros, uint32_t count) override;

  Status LogElapsedTime(uint32_t metric_id, uint32_t event_type_index,
                        const std::string& component,
                        int64_t elpased_micros) override;

  Status LogFrameRate(uint32_t metric_id, uint32_t event_type_index,
                      const std::string& component, float fps) override;

  Status LogMemoryUsage(uint32_t metric_id, uint32_t event_type_index,
                        const std::string& component, int64_t bytes) override;

  Status LogIntHistogram(uint32_t metric_id, uint32_t event_type_index,
                         const std::string& component,
                         HistogramPtr histogram) override;

  Status LogString(uint32_t metric_id, const std::string& str) override;

  // Note(rudominer) LogCustomEvent() is missing because it is still being
  // designed.

 private:
  friend class EventLogger;

  Status WriteObservation(const Observation2& observation,
                          std::unique_ptr<ObservationMetadata> metadata);

  void SetClock(std::unique_ptr<util::ClockInterface> clock) {
    clock_ = std::move(clock);
  }

  const Encoder* encoder_;
  encoder::ObservationStoreWriterInterface* observation_store_;
  encoder::ObservationStoreUpdateRecipient* update_recipient_;
  const util::EncryptedMessageMaker* observation_encrypter_;
  const ProjectContext* project_context_;
  std::unique_ptr<util::ClockInterface> clock_;
};

}  // namespace logger
}  // namespace cobalt

#endif  // COBALT_LOGGER_LOGGER_H_
