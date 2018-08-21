// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "logger/encoder.h"

#include <memory>
#include <string>

#include "./logging.h"
#include "./observation2.pb.h"
#include "algorithms/forculus/forculus_encrypter.h"
#include "algorithms/rappor/rappor_encoder.h"
#include "logger/project_context.h"
#include "logger/rappor_settings.h"

namespace cobalt {
namespace logger {

using ::cobalt::crypto::byte;
using ::cobalt::crypto::hash::DIGEST_SIZE;
using ::cobalt::encoder::ClientSecret;
using ::cobalt::encoder::SystemDataInterface;
using ::cobalt::forculus::ForculusEncrypter;
using ::cobalt::rappor::BasicRapporEncoder;
using ::cobalt::rappor::RapporEncoder;

namespace {
// Populates |*hash_out| with the SHA256 of |component|, unless |component|
// is empty in which case *hash_out is set to the empty string also. An
// empty string indicates that the component_name feature is not being used.
// We expect this to be a common case and in this case there is no point
// in using 32 bytes to represent the empty string. Returns true on success
// and false on failure (unexpected).
bool HashComponentNameIfNotEmpty(const std::string& component,
                                 std::string* hash_out) {
  CHECK(hash_out);
  if (component.empty()) {
    hash_out->resize(0);
    return true;
  }
  hash_out->resize(DIGEST_SIZE);
  return cobalt::crypto::hash::Hash(
      reinterpret_cast<const byte*>(component.data()), component.size(),
      reinterpret_cast<byte*>(&hash_out->front()));
}
}  // namespace

Encoder::Encoder(ClientSecret client_secret,
                 const encoder::SystemDataInterface* system_data)
    : client_secret_(client_secret), system_data_(system_data) {}

Encoder::Result Encoder::EncodeBasicRapporObservation(
    ProjectContext::MetricRef metric, const ReportDefinition* report,
    uint32_t day_index, uint32_t value_index, uint32_t num_categories) const {
  auto result = MakeObservation(metric, report, day_index);
  auto* observation = result.observation.get();
  auto* basic_rappor_observation = observation->mutable_basic_rappor();

  BasicRapporConfig basic_rappor_config;
  basic_rappor_config.set_prob_rr(kProbRR);
  basic_rappor_config.mutable_indexed_categories()->set_num_categories(
      num_categories);

  // compute heuristic for p and q.
  switch (report->local_privacy_noise_level()) {
    case ReportDefinition::NONE:
      basic_rappor_config.set_prob_0_becomes_1(kLocalPrivacyNoneProb0Becomes1);
      basic_rappor_config.set_prob_1_stays_1(kLocalPrivacyNoneProb1Stays1);
      break;

    case ReportDefinition::SMALL:
      basic_rappor_config.set_prob_0_becomes_1(kLocalPrivacySmallProb0Becomes1);
      basic_rappor_config.set_prob_1_stays_1(kLocalPrivacySmallProb1Stays1);
      break;

    case ReportDefinition::MEDIUM:
      basic_rappor_config.set_prob_0_becomes_1(
          kLocalPrivacyMediumProb0Becomes1);
      basic_rappor_config.set_prob_1_stays_1(kLocalPrivacyMediumProb1Stays1);
      break;

    case ReportDefinition::LARGE:
      basic_rappor_config.set_prob_0_becomes_1(kLocalPrivacyLargeProb0Becomes1);
      basic_rappor_config.set_prob_1_stays_1(kLocalPrivacyLargeProb1Stays1);
      break;

    default:
      LOG(ERROR) << "Invalid Cobalt config: Report " << report->report_name()
                 << " for metric " << metric.metric_name() << " in project "
                 << metric.project().DebugString()
                 << " does not have local_privacy_noise_level set to a "
                    "recognized value.";
      result.status = kInvalidConfig;
      return result;
  }

  BasicRapporEncoder basic_rappor_encoder(basic_rappor_config, client_secret_);
  ValuePart index_value;
  index_value.set_index_value(value_index);
  switch (basic_rappor_encoder.Encode(index_value, basic_rappor_observation)) {
    case rappor::kOK:
      break;

    case rappor::kInvalidConfig:
      LOG(ERROR) << "BasicRapporEncoder returned kInvalidConfig for: Report "
                 << report->report_name() << " for metric "
                 << metric.metric_name() << " in project "
                 << metric.project().DebugString() << ".";
      result.status = kInvalidConfig;
      return result;

    case rappor::kInvalidInput:
      LOG(ERROR) << "BasicRapporEncoder returned kInvalidInput for: Report "
                 << report->report_name() << " for metric "
                 << metric.metric_name() << " in project "
                 << metric.project().DebugString() << ".";
      result.status = kInvalidArguments;
      return result;
  }
  return result;
}

Encoder::Result Encoder::EncodeRapporObservation(
    ProjectContext::MetricRef metric, const ReportDefinition* report,
    uint32_t day_index, const std::string& str) const {
  auto result = MakeObservation(metric, report, day_index);
  auto* observation = result.observation.get();
  auto* rappor_observation = observation->mutable_string_rappor();

  RapporConfig rappor_config;
  rappor_config.set_num_hashes(kNumHashes);

  // Compute heuristic for num_cohorts.
  if (report->expected_population_size() == 0) {
    rappor_config.set_num_cohorts(kDefaultNumCohorts);
  } else if (report->expected_population_size() < kTinyPopulationSize) {
    rappor_config.set_num_cohorts(kTinyNumCohorts);
  } else if (report->expected_population_size() < kSmallPopulationSize) {
    rappor_config.set_num_cohorts(kSmallNumCohorts);
  } else if (report->expected_population_size() < kMediumPopulationSize) {
    rappor_config.set_num_cohorts(kMediumNumCohorts);
  } else {
    rappor_config.set_num_cohorts(kLargeNumCohorts);
  }

// Compute heuristic for num_bloom_bits.
  if (report->expected_string_set_size() == 0) {
    rappor_config.set_num_bloom_bits(kDefaultNumBits);
  } else if (report->expected_string_set_size() < kTinyNumCandidates) {
    rappor_config.set_num_bloom_bits(kTinyNumBits);
  } else if (report->expected_string_set_size() < kSmallNumCandidates) {
    rappor_config.set_num_bloom_bits(kSmallNumBits);
  } else if (report->expected_string_set_size() < kMediumNumCandidates) {
    rappor_config.set_num_bloom_bits(kMediumNumBits);
  } else {
    rappor_config.set_num_bloom_bits(kLargeNumBits);
  }

  // Compute heuristic for p and q.
  rappor_config.set_prob_rr(kProbRR);
  switch (report->local_privacy_noise_level()) {
    case ReportDefinition::NONE:
      rappor_config.set_prob_0_becomes_1(kLocalPrivacyNoneProb0Becomes1);
      rappor_config.set_prob_1_stays_1(kLocalPrivacyNoneProb1Stays1);
      break;

    case ReportDefinition::SMALL:
      rappor_config.set_prob_0_becomes_1(kLocalPrivacySmallProb0Becomes1);
      rappor_config.set_prob_1_stays_1(kLocalPrivacySmallProb1Stays1);
      break;

    case ReportDefinition::MEDIUM:
      rappor_config.set_prob_0_becomes_1(kLocalPrivacyMediumProb0Becomes1);
      rappor_config.set_prob_1_stays_1(kLocalPrivacyMediumProb1Stays1);
      break;

    case ReportDefinition::LARGE:
      rappor_config.set_prob_0_becomes_1(kLocalPrivacyLargeProb0Becomes1);
      rappor_config.set_prob_1_stays_1(kLocalPrivacyLargeProb1Stays1);
      break;

    default:
      LOG(ERROR) << "Invalid Cobalt config: Report " << report->report_name()
                 << " for metric " << metric.metric_name() << " in project "
                 << metric.project().DebugString()
                 << " does not have local_privacy_noise_level set to a "
                    "recognized value.";
      result.status = kInvalidConfig;
      return result;
  }

  RapporEncoder rappor_encoder(rappor_config, client_secret_);
  ValuePart string_value;
  string_value.set_string_value(str);
  switch (rappor_encoder.Encode(string_value, rappor_observation)) {
    case rappor::kOK:
      break;

    case rappor::kInvalidConfig:
      LOG(ERROR) << "RapporEncoder returned kInvalidConfig for: Report "
                 << report->report_name() << " for metric "
                 << metric.metric_name() << " in project "
                 << metric.project().DebugString() << ".";
      result.status = kInvalidConfig;
      return result;

    case rappor::kInvalidInput:
      LOG(ERROR) << "RapporEncoder returned kInvalidInput for: Report "
                 << report->report_name() << " for metric "
                 << metric.metric_name() << " in project "
                 << metric.project().DebugString() << ".";
      result.status = kInvalidArguments;
      return result;
  }
  return result;
}

Encoder::Result Encoder::EncodeForculusObservation(
    ProjectContext::MetricRef metric, const ReportDefinition* report,
    uint32_t day_index, const std::string& str) const {
  auto result = MakeObservation(metric, report, day_index);
  auto* observation = result.observation.get();
  auto* forculus_observation = observation->mutable_forculus();
  ForculusConfig forculus_config;
  if (report->threshold() < 2) {
    LOG(ERROR) << "Invalid Cobalt config: Report " << report->report_name()
               << " for metric " << metric.metric_name() << " in project "
               << metric.project().DebugString()
               << " has an invalid value for |threshold|.";
    result.status = kInvalidConfig;
    return result;
  }
  forculus_config.set_threshold(report->threshold());
  forculus_config.set_epoch_type(DAY);
  ValuePart string_value;
  string_value.set_string_value(str);
  ForculusEncrypter forculus_encrypter(
      forculus_config, metric.project().customer_id(),
      metric.project().project_id(), metric.metric_id(), "", client_secret_);

  switch (forculus_encrypter.EncryptValue(string_value, day_index,
                                          forculus_observation)) {
    case ForculusEncrypter::kOK:
      break;

    case ForculusEncrypter::kInvalidConfig:
      LOG(ERROR) << "ForculusEncrypter returned kInvalidConfig for: Report "
                 << report->report_name() << " for metric "
                 << metric.metric_name() << " in project "
                 << metric.project().DebugString() << ".";
      result.status = kInvalidConfig;
      return result;

    case ForculusEncrypter::kEncryptionFailed:
      LOG(ERROR) << "ForculusEncrypter returned kEncryptionFailed for: Report "
                 << report->report_name() << " for metric "
                 << metric.metric_name() << " in project "
                 << metric.project().DebugString() << ".";
      result.status = kOther;
  }
  return result;
}

Encoder::Result Encoder::EncodeIntegerEventObservation(
    ProjectContext::MetricRef metric, const ReportDefinition* report,
    uint32_t day_index, uint32_t event_type_index, const std::string component,
    int64_t value) const {
  auto result = MakeObservation(metric, report, day_index);
  auto* observation = result.observation.get();
  auto* integer_event_observation = observation->mutable_numeric_event();
  integer_event_observation->set_event_type_index(event_type_index);
  if (!HashComponentNameIfNotEmpty(
          component,
          integer_event_observation->mutable_component_name_hash())) {
    LOG(ERROR) << "Hashing the component name failed for: Report "
               << report->report_name() << " for metric "
               << metric.metric_name() << " in project "
               << metric.project().DebugString() << ".";
    result.status = kOther;
  }
  integer_event_observation->set_value(value);
  return result;
}

Encoder::Result Encoder::EncodeHistogramObservation(
    ProjectContext::MetricRef metric, const ReportDefinition* report,
    uint32_t day_index, uint32_t event_type_index, const std::string component,
    HistogramPtr histogram) const {
  auto result = MakeObservation(metric, report, day_index);
  auto* observation = result.observation.get();
  auto* histogram_observation = observation->mutable_histogram();
  histogram_observation->set_event_type_index(event_type_index);
  if (!HashComponentNameIfNotEmpty(
          component, histogram_observation->mutable_component_name_hash())) {
    LOG(ERROR) << "Hashing the component name failed for: Report "
               << report->report_name() << " for metric "
               << metric.metric_name() << " in project "
               << metric.project().DebugString() << ".";
    result.status = kOther;
  }
  histogram_observation->mutable_buckets()->Swap(histogram.get());
  return result;
}

Encoder::Result Encoder::MakeObservation(ProjectContext::MetricRef metric,
                                         const ReportDefinition* report,
                                         uint32_t day_index) const {
  Result result;
  result.status = kOK;
  result.observation = std::make_unique<Observation2>();
  auto* observation = result.observation.get();
  result.metadata = std::make_unique<ObservationMetadata>();
  auto* metadata = result.metadata.get();

  // Generate the random_id field. Currently we use 8 bytes but our
  // infrastructure allows us to change that in the future if we wish to. The
  // random_id is used by the Analyzer Service as part of a unique row key
  // for the observation in the Observation Store.
  static const size_t kNumRandomBytes = 8;
  observation->set_allocated_random_id(new std::string(kNumRandomBytes, 0));
  random_.RandomString(observation->mutable_random_id());

  metadata->set_customer_id(metric.project().customer_id());
  metadata->set_project_id(metric.project().project_id());
  metadata->set_metric_id(metric.metric_id());
  metadata->set_report_id(report->id());
  metadata->set_day_index(day_index);

  if (system_data_) {
    const auto& profile = system_data_->system_profile();
    if (report->system_profile_field_size() == 0) {
      metadata->mutable_system_profile()->set_board_name(profile.board_name());
      metadata->mutable_system_profile()->set_product_name(
          profile.product_name());
    } else {
      for (const auto& field : report->system_profile_field()) {
        switch (field) {
          case SystemProfileField::OS:
            metadata->mutable_system_profile()->set_os(profile.os());
            break;
          case SystemProfileField::ARCH:
            metadata->mutable_system_profile()->set_arch(profile.arch());
            break;
          case SystemProfileField::BOARD_NAME:
            metadata->mutable_system_profile()->set_board_name(
                profile.board_name());
            break;
          case SystemProfileField::PRODUCT_NAME:
            metadata->mutable_system_profile()->set_product_name(
                profile.product_name());
            break;
        }
      }
    }
  }

  return result;
}

}  // namespace logger
}  // namespace cobalt
