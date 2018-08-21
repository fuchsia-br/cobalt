// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_LOGGER_RAPPOR_SETTINGS_H_
#define COBALT_LOGGER_RAPPOR_SETTINGS_H_

namespace cobalt {
namespace logger {

// This file contains constants that are used in the logic for setting
// RAPPOR configuration parameters based on the user's settings in the
// Cobalt config.

// TODO(rudominer) Have mironov@ review this file.

////////////////////////////    p, q and f    //////////////////////////////////

// This is relevent to Basic RAPPOR (via the SIMPLE_OCCURRENCE_COUNT
// Report type) and String RAPPOR (via the HIGH_FREQUENCY_STRING_COUNTS Report
// type.)

// Currently we do not support RAPPOR's PRR in Cobalt.
static const float kProbRR = 0.0;

// The mapping from different values of LocalPrivacyNoiseLevel to
// RAPPOR parameters p and q. The user sets LocalPrivacyNoiseLevel on
// a per-report basis. Here we refer to p as Prob0Becomes1 and q as Prob1Stays1.

// NONE
static const float kLocalPrivacyNoneProb0Becomes1 = 0.0;
static const float kLocalPrivacyNoneProb1Stays1 = 1.0;

// SMALL
static const float kLocalPrivacySmallProb0Becomes1 = 0.01;
static const float kLocalPrivacySmallProb1Stays1 = 0.99;

// MEDIUM
static const float kLocalPrivacyMediumProb0Becomes1 = 0.1;
static const float kLocalPrivacyMediumProb1Stays1 = 0.9;

// LARGE
static const float kLocalPrivacyLargeProb0Becomes1 = 0.25;
static const float kLocalPrivacyLargeProb1Stays1 = 0.75;

/////////////////////////  h = num_hashes   //////////////////////////////////
// We always use h = 2.
static const size_t kNumHashes = 2;

// This is relevent to Basic RAPPOR (via the SIMPLE_OCCURRENCE_COUNT
// Report type) and String RAPPOR (via the HIGH_FREQUENCY_STRING_COUNTS Report
// type.)

////////////////////////    m = num_cohorts    ///////////////////////////////

// This is relevent to String RAPPOR (via the HIGH_FREQUENCY_STRING_COUNTS
// Report type.)

// |expected_population_size| is the user's estimate of the number of Fuchsia
// devices from which data is collected.

// The mapping from expected_population_size to num_cohorts

// This is the number of cohorts to use if expected_population_size is not set.
static const size_t kDefaultNumCohorts = 50;

// For estimated_population_size < 100 we use 5 cohorts.
static const size_t kTinyPopulationSize = 100;
static const size_t kTinyNumCohorts = 5;

// For 100 <= estimated_population_size < 1000 we use 10 cohorts.
static const size_t kSmallPopulationSize = 1000;
static const size_t kSmallNumCohorts = 10;

// For 1,000 <= estimated_population_size < 10,000 we use 50 cohorts.
static const size_t kMediumPopulationSize = 10000;
static const size_t kMediumNumCohorts = 50;

// For estimated_population_size >= 10,000 we use 100 cohorts
static const size_t kLargeNumCohorts = 100;

////////////////////////    k = num_bloom_bits    //////////////////////////////

// This is relevent to String RAPPOR (via the HIGH_FREQUENCY_STRING_COUNTS
// Report type.)

// |expected_string_set_size| is the user's estimate of the number of candidate
// strings. This cannot be changed once data collection begins.

// The mapping from expected_string_set_size to num_bloom_bits

// This is the number of bits to use if expected_string_set_size is not set.
static const size_t kDefaultNumBits = 32;

// For estimated_string_set_size < 100 we use 8 bits.
static const size_t kTinyNumCandidates = 100;
static const size_t kTinyNumBits = 8;

// For 100 <= estimated_string_set_size < 1000 we use 16 bits.
static const size_t kSmallNumCandidates = 1000;
static const size_t kSmallNumBits = 16;

// For 1,000 <= estimated_string_set_size < 10,000 we use 64 bits.
static const size_t kMediumNumCandidates = 10000;
static const size_t kMediumNumBits = 64;

// For estimated_string_set_size >= 10,000 we use 128 bits
static const size_t kLargeNumBits = 128;

}  // namespace logger
}  // namespace cobalt

#endif  // COBALT_LOGGER_RAPPOR_SETTINGS_H_
