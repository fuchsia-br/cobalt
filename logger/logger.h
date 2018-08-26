// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_LOGGER_LOGGER_H_
#define COBALT_LOGGER_LOGGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "./event.pb.h"
#include "./observation2.pb.h"
#include "config/metric_definition.pb.h"
#include "encoder/client_secret.h"
#include "encoder/observation_store.h"
#include "encoder/observation_store_update_recipient.h"
#include "logger/encoder.h"
#include "logger/project_context.h"
#include "logger/status.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"
#include "util/clock.h"
#include "util/encrypted_message_util.h"

namespace cobalt {
namespace logger {

// Logger is the client-facing interface to Cobalt.
//
// LoggerInterface is an abstract interface to Logger that allows Logger to
// be mocked out in tests.
class LoggerInterface {
 public:
  virtual ~LoggerInterface() = default;

  // Logs the fact that an event has occurred.
  //
  // |metric_id| ID of the Metric the logged Event will belong to. It must
  // be one of the Metrics from the ProjectContext passed to the constructor,
  // and it must be of type EVENT_OCCURRED.
  //
  // |event_type_index| The index of the event type that occurred. The indexed
  // set of all event types is specified in the Metric definition. Use 0
  // if there is no natural notion of event type.
  //
  virtual Status LogEvent(uint32_t metric_id, uint32_t event_type_index) = 0;

  // Logs that an event has occurred a given number of times.
  //
  // |metric_id| ID of the Metric the logged Event will belong to. It must
  // be one of the Metrics from the ProjectContext passed to the constructor,
  // and it must be of type EVENT_COUNT.
  //
  // |event_type_index| The index of the event type that occurred. The indexed
  // set of all event types is specified in the Metric definition. Use 0
  // if there is no natural notion of event type.
  //
  // |component| Optionally, a component associated with the event may also be
  // logged. Use the empty string if there is no natural notion of component.
  //
  // |period_duration_micros| Optionally, the period of time over which
  // the |count| events occurred may be logged. If this is not relevant the
  // value may be set to 0. Otherwise specify the period duration as a number
  // of microseconds.
  //
  // |count| The number of times the event occurred. One may choose to
  // always set this value to 1 and always set |period_duration_micros| to 0
  // in order to achieve a semantics similar to the LogEventOccurred() method,
  // but with a |component|.
  virtual Status LogEventCount(uint32_t metric_id, uint32_t event_type_index,
                               const std::string& component,
                               int64_t period_duration_micros,
                               uint32_t count) = 0;

  // Logs that an event lasted a given amount of time.
  //
  // |metric_id| ID of the Metric the logged Event will belong to. It must
  // be one of the Metrics from the ProjectContext passed to the constructor,
  // and it must be of type ELAPSED_TIME.
  //
  // |event_type_index| The index of the event type that occurred. The indexed
  // set of all event types is specified in the Metric definition. Use 0
  // if there is no natural notion of event type.
  //
  // |component| Optionally, a component associated with the event may also be
  // logged. Use the empty string if there is no natural notion of component.
  //
  // |elapsed_micros| The elapsed time of the event, specified as a number
  // of microseconds.
  virtual Status LogElapsedTime(uint32_t metric_id, uint32_t event_type_index,
                                const std::string& component,
                                int64_t elpased_micros) = 0;

  // Logs a measured average frame rate.
  //
  // |metric_id| ID of the Metric the logged Event will belong to. It must
  // be one of the Metrics from the ProjectContext passed to the constructor,
  // and it must be of type FRAME_RATE.
  //
  // |event_type_index| The index of the event type that occurred. The indexed
  // set of all event types is specified in the Metric definition. Use 0
  // if there is no natural notion of event type.
  //
  // |component| Optionally, a component associated with the event may also be
  // logged. Use the empty string if there is no natural notion of component.
  //
  // |fps| The average-frame rate in frames-per-second.
  virtual Status LogFrameRate(uint32_t metric_id, uint32_t event_type_index,
                              const std::string& component, float fps) = 0;
  // Logs a measured memory usage.
  //
  // |metric_id| ID of the Metric the logged Event will belong to. It must
  // be one of the Metrics from the ProjectContext passed to the constructor,
  // and it must be of type MEMORY_USAGE.
  //
  // |event_type_index| The index of the event type that occurred. The indexed
  // set of all event types is specified in the Metric definition. Use 0
  // if there is no natural notion of event type.
  //
  // |component| Optionally, a component associated with the event may also be
  // logged. Use the empty string if there is no natural notion of component.
  //
  // |bytes| The memory used, in bytes.
  virtual Status LogMemoryUsage(uint32_t metric_id, uint32_t event_type_index,
                                const std::string& component,
                                int64_t bytes) = 0;

  // Logs a histogram over a set of integer buckets. The meaning of the
  // Metric and the buckets is specified in the Metric definition.
  //
  // |metric_id| ID of the Metric the logged Event will belong to. It must
  // be one of the Metrics from the ProjectContext passed to the constructor,
  // and it must be of type INT_HISTOGRAM.
  //
  // |event_type_index| The index of the event type that occurred. The indexed
  // set of all event types is specified in the Metric definition. Use 0
  // if there is no natural notion of event type.
  //
  // |component| Optionally, a component associated with the event may also be
  // logged. Use the empty string if there is no natural notion of component.
  //
  // |histogram| The histogram to log. Each HistogramBucket gives the count
  //  for one bucket of the histogram. The definitions of the buckets is
  //  given in the Metric definition.
  virtual Status LogIntHistogram(uint32_t metric_id, uint32_t event_type_index,
                                 const std::string& component,
                                 HistogramPtr histogram) = 0;

  // Logs the fact that a given string was used, in a specific context.
  // The semantics of the context and the string is specified in the
  // Metric definition.
  //
  //  This method is intended to be used in the following situation:
  //  * The string s being logged does not contain PII or passwords.
  //  * The set S of all possible strings that may be logged is large.
  //    If the set S is small consider using LogEvent() instead.
  //  * The ultimate data of interest is the statistical distribution of the
  //    most commonly used strings from S over the population of all Fuchsia
  //    devices.
  //
  // |metric_id| ID of the Metric the logged Event will belong to. It must
  // be one of the Metrics from the ProjectContext passed to the constructor,
  // and it must be of type STRING_USED.
  //
  // |str| The human-readable string to log.
  virtual Status LogString(uint32_t metric_id, const std::string& str) = 0;
};

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
