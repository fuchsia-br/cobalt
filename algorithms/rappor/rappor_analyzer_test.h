// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_ALGORITHMS_RAPPOR_RAPPOR_ANALYZER_TEST_H_
#define COBALT_ALGORITHMS_RAPPOR_RAPPOR_ANALYZER_TEST_H_

#include "algorithms/rappor/rappor_analyzer.h"

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "algorithms/rappor/rappor_encoder.h"
#include "algorithms/rappor/rappor_test_utils.h"
#include "encoder/client_secret.h"
#include "third_party/eigen/Eigen/SparseQR"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace cobalt {
namespace rappor {

class RapporAnalyzerTest : public ::testing::Test {
 protected:
  // Sets the member variable analyzer_ to a new RapporAnalyzer configured
  // with the given arguments and the current values of prob_0_becomes_1_,
  // prob_1_stays_1_.
  void SetAnalyzer(uint32_t num_candidates, uint32_t num_bloom_bits,
                   uint32_t num_cohorts, uint32_t num_hashes);

  void BuildCandidateMap();

  // This should be invoked after BuildCandidateMap. It returns the bit index
  // within the CandidateMap for the given |candidate_index|, |cohort_index|,
  // and |hash_index|.
  uint16_t GetCandidateMapValue(uint16_t candidate_index, uint16_t cohort_index,
                                uint16_t hash_index);

  // Builds and returns a bit string (i.e. a string of ASCII '0's and '1's)
  // representing the Bloom filter implicitly stored within the CandidateMap
  // for the given |candidate_index| and |cohort_index|.
  std::string BuildBitString(uint16_t candidate_index, uint16_t cohort_index);

  const Eigen::SparseMatrix<float, Eigen::RowMajor>& candidate_matrix() {
    return analyzer_->candidate_matrix_;
  }

  void AddObservation(uint32_t cohort, std::string binary_string);

  void ExtractEstimatedBitCountRatios(Eigen::VectorXf* est_bit_count_ratios);

  void ExtractEstimatedBitCountRatiosAndStdErrors(
      Eigen::VectorXf* est_bit_count_ratios,
      std::vector<float>* est_std_errors);

  void AddObservationsForCandidates(const std::vector<int>& candidate_indices);

  // Generate a random number from power law distribution on the interval
  // [|left|,|right|] with given |exponent|.
  int GenerateNumberFromPowerLaw(const double left, const double right,
                                 const double exponent);

  // Generate a "map" of shuffled Ids, that is, a vector of size
  // |num_candidates| containing exactly the numbers 0,1,...,num_canidates - 1,
  // in a random order.
  std::vector<int> GenerateRandomMapOfIds(const int num_candidates);

  std::vector<int> CountsEstimatesFromResults(
      const std::vector<CandidateResult>& results);

  Eigen::VectorXf VectorFromCounts(
      const std::vector<int>& exact_candidate_counts);

  // Checks how well the |exact_candidate_counts| reproduces the right hand side
  // of the equation solved by Analyze(). The function prints the value of
  // d == || A * x_s - b || / || b || where x_s == |exact_candidate_counts| /
  // num_observations, b is the vector of the estimated bit count ratios from
  // ExactEstimatedBitCountRatios(), and A == analyzer_->candidate_matrix_ (it
  // must be valid, the function does not build A).
  // The purpose of this function is to assess how much information is lost by
  // both encoding and the assumption that cohorts have equal ratios. d == 0
  // means that exact_candidate_count is among the solutions to the problem
  // being solved, so values much larger than 0 suggest that getting the right
  // counts in the Analyzer is difficult (if exact_candidate_counts is our a
  // priori knowledge of the solution). Note: d does not account for
  // non-uniqeness of the solution of A * x_s  = b, just checks how well the
  // given vector solves it.
  void CheckExactSolution(const std::vector<int>& exact_candidate_counts);

  void PrintTrueCountsAndEstimates(
      const std::string& case_label, uint32_t num_candidates,
      const std::vector<CandidateResult>& results,
      const std::vector<int>& true_candidate_counts);

  // Assess utility of |results|. The informal measure suggested by mironov is:
  // "Largest n such that at most 10% of the n highest hitters are identified as
  // such incorrectly (are false positives)". Obviously, this makes sense only
  // for n in some range (it may unjusty suggest that the results are bad for n
  // too small, or for n too large, that they are good when in fact they are
  // not). Also, 10% is arbitrary. So instead, we just compute the false
  // positive rates for n in some grid set. We also print the total number of
  // nonzero estimates identified.
  void AssessUtility(const std::vector<CandidateResult>& results,
                     const std::vector<int>& true_candidate_counts);

  // Checks correctness of the solution stored in |results| in an explicit way.
  // This is not an automated test but rather a tool to manually assess the
  // minimizer quality.
  //
  // Assumes that *analyzer_ contains minimizer data from previous run.
  // The problem is (as formulated in lossmin library):
  //                        min L(beta) ==
  // 1/(2*N) * ||X * beta - y||_2^2 + 1/2 * l2 *||beta||_2^2 + l1 *||beta||_1,
  // with variable beta. We assume l1,l2 >= 0.
  // In our case, X == candidate_matrix(),
  // beta == |results| / analyzer_->bit_counter().num_observations(),
  // y == est_bit_count_ratios (observed ratios computed by calling
  // analyzer_->ExtractEstimatedBitCountRatios(&est_bit_count_ratios)),
  // l1 == analyzer_->minimizer_data.l1,
  // l2 == analyzer_->minimizer_data.l2,
  // N == canididate_matrix.rows().
  //
  // Let grad denote the gradient of
  // F(beta) = 1/(2*N) * ||X * beta - y||_2^2 + 1/2 * l2 ||beta||_2^2.
  // Note that grad == 1/N * X^T(X * beta  - y) + beta.
  //
  // The KKT condition (in exact arithmetic) can be
  // written explicitly in the following way:
  // If beta_i > 0, then grad_i == -l1
  // If beta_i < 0, then grad_i == l1
  // If beta_i == 0, then  -l1 <= grad_i <= l1.
  //
  // A point beta is a minimizer iff the KKT condition holds for beta
  // (this minimizer need not be unique though).
  //
  // We check the KKT condition up to a given accuracy:
  // |tol_cand| is the absolute tolerance at which we measure values of beta
  // |tol_grad| is the absolute tolerance at which we measure values of grad
  //
  // Thus, beta_i > 0 is replaced by beta_i > tol_cand, beta_i < 0 is replaced
  // by beta_i < -tol_cand, grad_i == +/- l1 is replaced by
  // grad_i <=/>= +/- l1 +/- tol_grad
  // and similarly for the inequality check.
  // tol_cand and tol_grad should be consistent with implementation of
  // lossmin::LossMinimizer::ConvergenceCheck but other values can be useful
  // for testing.
  // TODO(bazyli) make sure these checks remain consistent with lossmin and
  // floatig point arithmetics.
  //
  // The test also prints quantitative violation of KKT condition as a mean
  // violation per coordinate.
  void CheckSolutionCorrectness(const float tol_cand, const float tol_grad,
                                const std::vector<CandidateResult>& results);

  // Computes the least squares fit on the candidate matrix using QR,
  // for the given rhs in |est_bit_count_ratios| and saves it to |results|
  grpc::Status ComputeLeastSquaresFitQR(
      const Eigen::VectorXf& est_bit_count_ratios,
      std::vector<CandidateResult>* results);

  // Runs a simple least squares problem for Ax = b on the candidate matrix
  // using QR algorithm from eigen library; this is to see the results
  // without penalty terms (note: in an overdetermined system the solution
  // is not unique so this is more a helper testing function to cross-check
  // the behavior of regression without penalty)
  void RunSimpleLinearRegressionReference(
      const std::string& case_label, uint32_t num_candidates,
      uint32_t num_bloom_bits, uint32_t num_cohorts, uint32_t num_hashes,
      std::vector<int> candidate_indices,
      std::vector<int> true_candidate_counts);

  // Invokes the Analyze() method using the given parameters. Checks that
  // the algorithms converges and that the result vector has the correct length.
  void ShortExperimentWithAnalyze(const std::string& case_label,
                                  uint32_t num_candidates,
                                  uint32_t num_bloom_bits, uint32_t num_cohorts,
                                  uint32_t num_hashes,
                                  std::vector<int> candidate_indices,
                                  std::vector<int> true_candidate_counts,
                                  const bool print_estimates);

  // Same as ShortExperimentWithAnalyze() but also measures the time taken by
  // Analyze(), calls CheckExactSolution() to assess the loss of information,
  // and calls AssessUtility() to compare the results with true counts.
  void LongExperimentWithAnalyze(const std::string& case_label,
                                 uint32_t num_candidates,
                                 uint32_t num_bloom_bits, uint32_t num_cohorts,
                                 uint32_t num_hashes,
                                 std::vector<int> candidate_indices,
                                 std::vector<int> true_candidate_counts,
                                 const bool print_estimates);

  // Similar to ShortExperimentWithAnalyze except it also
  // computes the estimates for both Analyze and simple regression using QR,
  // which is computed on exactly the same system.
  void CompareAnalyzeToSimpleRegression(const std::string& case_label,
                                        uint32_t num_candidates,
                                        uint32_t num_bloom_bits,
                                        uint32_t num_cohorts,
                                        uint32_t num_hashes,
                                        std::vector<int> candidate_indices,
                                        std::vector<int> true_candidate_counts);

  RapporConfig config_;
  std::unique_ptr<RapporAnalyzer> analyzer_;

  RapporCandidateList candidate_list_;

  // By default this test uses p=0, q=1. Individual tests may override this.
  double prob_0_becomes_1_ = 0.0;
  double prob_1_stays_1_ = 1.0;

  // Random device
  std::random_device random_dev_;
};

}  // namespace rappor
}  // namespace cobalt

#endif  //  COBALT_ALGORITHMS_RAPPOR_RAPPOR_ANALYZER_TEST_H_
