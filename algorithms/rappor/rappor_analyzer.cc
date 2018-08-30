// Copyright 2017 The Fuchsia Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "algorithms/rappor/rappor_analyzer.h"

#include <glog/logging.h>
#include <algorithm>
#include <cmath>
#include <random>

#include "algorithms/rappor/rappor_encoder.h"
#include "third_party/lossmin/lossmin/losses/inner-product-loss-function.h"
#include "third_party/lossmin/lossmin/minimizers/gradient-evaluator.h"
#include "third_party/lossmin/lossmin/minimizers/parallel-boosting-with-momentum.h"
#include "util/crypto_util/hash.h"
#include "util/log_based_metrics.h"

namespace cobalt {
namespace rappor {

// Stackdriver metric contants
namespace {
const char kAnalyzeFailure[] = "rappor-analyzer-analyze-failure";
}  // namespace

using crypto::byte;

RapporAnalyzer::RapporAnalyzer(const RapporConfig& config,
                               const RapporCandidateList* candidates)
    : bit_counter_(config), config_(bit_counter_.config()) {
  candidate_map_.candidate_list = candidates;
  // candidate_map_.candidate_cohort_maps remains empty for now. It
  // will be populated by BuildCandidateMap.
}

bool RapporAnalyzer::AddObservation(const RapporObservation& obs) {
  VLOG(5) << "RapporAnalyzer::AddObservation() cohort=" << obs.cohort();
  return bit_counter_.AddObservation(obs);
}

grpc::Status RapporAnalyzer::Analyze(
    std::vector<CandidateResult>* results_out) {
  CHECK(results_out);

  // TODO(rudominer) Consider inserting here an analysis of the distribution
  // of the number of Observations over the set of cohorts. The mathematics
  // of our algorithm below assumes that this distribution is uniform. If
  // it is not uniform in practice this may indicate a problem with client-
  // side code and we may wish to take some corrective action.

  auto status = BuildCandidateMap();
  if (!status.ok()) {
    return status;
  }

  // est_bit_count_ratios is the right-hand side vector b from the equation Ax =
  // b that we are estimating, and est_std_errors are the corresponding standard
  // errors. See comments on the declaration of
  // ExtractEstimatedBitCountRatiosAndStdErrors() for a description of this
  // vector.
  Eigen::VectorXd est_bit_count_ratios;
  std::vector<double> est_std_errors;
  ExtractEstimatedBitCountRatiosAndStdErrors(&est_bit_count_ratios,
                                             &est_std_errors);

  // Note(rudominer) The GradientEvaluator constructor takes a
  // const LabelSet& parameter but est_bit_count_ratios is a
  // VectorXf. These types are different:
  // LabelSet = Matrix<float, Dynamic, Dynamic, RowMajor>
  // VectorXf = Matrix<float, Dynamic, 1>
  // These are different in two ways: VectorXf has a static number of columns
  // and VectorXf uses ColumnMajor order.
  // Here we copy est_bit_count_ratios to a label set before passing it to
  // the GradientEvaluator. This works fine. Some notes about this:
  // (1) Eigen defines copy constructors that do the right thing.
  // (2) There is no compilation error if we pass est_bit_count_ratios directly
  //     to the GradientEvaluator constructor. Somehow this works--I'm not sure
  //     why. But doing this leads to some unknown memory corruption which
  //     causes very bad non-deterministic runtime behavior. Be careful not
  //     to do this.
  // (3) We could avoid doing a copy here by just declaring est_bit_count_ratios
  //     as a LabelSet to begin with. But I don't like doing this because it
  //     makes the code less understandable to define a known column vector
  //     as a matrix with a dynamic number of columns in RowMajor order.

  lossmin::LabelSet as_label_set = est_bit_count_ratios;
  lossmin::InstanceSet candidate_matrix = candidate_matrix_;
  lossmin::LinearRegressionLossFunction loss_function;
  lossmin::GradientEvaluator grad_eval(candidate_matrix, as_label_set,
                                       &loss_function);
  // In the first step, we compute the lasso path. That is,
  // we compute the solutions to a sequence of lasso problems
  // with decreasing values of l1 penalty. The path is linear, that is the
  // difference between the lasso penalty of each two consecutive subproblems is
  // constant. The solution to each problem gives a "warm start" for the next
  // one. (The initial, largest value of l1 is the smallest value such that the
  // solution to the lasso problem is zero. This value of l1 is equal to the
  // infinity norm of the unpenalized objective gradient at zero). This is the
  // standard way to compute lasso and has a number of advantages cited in
  // literature. It is more numerically stable and more efficient than computing
  // just the last problem; it gives the entire path of solutions and therefore
  // can be used to choose the most meaningful value of penalty (in our case we
  // do not really know it a priori).
  // Note(bazyli): the R glmnet library uses logarithmic path; I found, however,
  // that it often introduces (too) many nonzeros in the initial subproblems of
  // the lasso path. Since we are interested in heavy hitters, we may not need
  // to run the entire path and want to be more conservative as to how quickly
  // the new nonzeros are identified.

  // Initialize the solution vector to zero vector for lasso path.
  const int num_candidates = candidate_matrix.cols();
  lossmin::Weights est_candidate_weights =
      lossmin::Weights::Zero(num_candidates);

  // Construct the minimizer with zero penalty (we will set penalties later).
  // Also compute initial gradient (this will be used to get initilal l1
  // penalty and convergence threshold).
  lossmin::ParallelBoostingWithMomentum minimizer(0.0, 0.0, grad_eval);
  lossmin::Weights initial_gradient = lossmin::Weights::Zero(num_candidates);
  minimizer.SparseInnerProductGradient(est_candidate_weights,
                                       &initial_gradient);

  // Set the parameters for the convergence algorithm.
  // Note(bazyli)
  // 1. To improve the accuracy of the solution you may want to
  // set kRelativeConvergenceThreshold,
  // kRelativeInLassoPathConvergenceThreshold, and kSimpleConvergenceThreshold
  // to smaller values, at the expense of more computations (epochs) performed.
  // See their descriptions below.
  // 2. You can set kZeroThreshold to the value you think is best (smaller
  // than what you would deem negligible); however, do not get close to double
  // presicion accuracy (1e-16).
  // 3. You can change kMaxNonzeroCoefficients depending on how many largest
  // coefficients you care about; smaller number means fewer iterations (shorter
  // lasso path).
  // 4. If you often bump into the error "The lasso path did not reach the last
  // subproblem.", it may be a sign that kMaxEpochs is too strict (i.e. too low)
  // and you may want to increase it.
  // 5. Modify other parameters with caution.
  //
  // kRelativeConvergenceThreshold is the improvement that we expect to achieve
  // at convergence, relative to initial gradient. That is, if ||g|| is the
  // initial 2-norm norm of the gradient, we expect the final (i.e. after the
  // last lasso problem) total measure of KKT violation to be equal to
  // kRelativeConvergenceThreshold * ||g||. Value smaller than 1e-8 gives a full
  // (single precision) convergence and is probably overshooting because the
  // purpose of the first step is mostly to identify potential nonzero
  // coefficients. Something between 1e-5 and 1e-7 should be enough.
  // Even larger value can be chosen for performance reasons. This accuracy will
  // be used only in the final lasso subproblem.
  // kRelativeInLassoPathConvergenceThreshold has the same interpretaion but
  // will be used inside the lasso path (before the last subproblem). We
  // probably want to set this to be slightly (e.g. ten times) less restrictive
  // (larger) than kRelativeConvergenceThreshold for efficiency because the
  // solutions inside the lasso path serve as a "warm-up" before the last
  // subproblem; this subproblem, on the other hand, can be more accurate and
  // should benefit more from "momentum" computations in the algorithm.
  // kSimpleConvergenceThreshold == d means that the algorithm will stop if the
  // best relative improvement between two consecutive measures of the objective
  // in the last kConvergenceMeasures measures, is below d. In other words,
  // relative improvements less than d will cause the minimizer to return.
  // See the description of kConvergenceEpochs and
  // kLossEpochs below. A rule-of-thumb value for kSimpleConvergenceThreshold
  // can be the inverse of the kMaxEpochs below.
  //
  // Note: The actual absolute values of convergence
  // thresholds used in the algorithm are capped in case ||g|| is almost
  // zero in the first place (see below).
  static const double kRelativeConvergenceThreshold = 1e-5;
  static const double kRelativeInLassoPathConvergenceThreshold = 1e-4;
  static const double kSimpleConvergenceThreshold = 1e-5;
  // kZeroThreshold is the (relative) proportion below which we assume a
  // candidate count to be effectively zero; that is, we assume a candidate to
  // be zero if its estimated count is below kZeroThreshold of the
  // bit_counter_.num_observations(). It cannot be exactly zero for
  // performance reasons (also, exact zero makes little if any sense
  // numerically). Also, smaller values might be more difficult for individual
  // runs of the minimizer.
  static const double kZeroThreshold = 1e-6;
  // kNumLassoSteps is the number of problems solved in the lasso path (the R
  // library glmnet default value is 100); it is not true that more steps
  // will take more time: there should be a "sweet spot" and definitely this
  // number should not be too small (probably something between 50 and 500).
  static const int kNumLassoSteps = 100;
  // kL1maxToL1minRatio is the ratio between the first and the last l1 penalty
  // value in the lasso path; should probably be something between 1e-6 and
  // 1e-3. (the R library glmnet uses 1e-3 by default).
  static const double kL1maxToL1minRatio = 1e-3;
  // kL2toL1Ratio is the ratio between l1 and l2 penalty.
  // Although pure lasso does not include any l2 penalty, a tiny bit can improve
  // stability. A value of kL2toL1Ratio less or equal to 1e-2 should not affect
  // the interpretation of the solution.
  static const double kL2toL1Ratio = 1e-2;
  // kLossEpochs denotes how often the current objective value is recorded in
  // the solver (it will be recorded every kLossEpochs epochs).
  // kConvergenceMeasures denotes how often the minimizer checks convergence.
  // The algorithm will check convergence
  // every (kLossEpochs * kConvergenceEpochs) epochs and terminate if at least
  // one of the following is true:
  // 1. The algorithm reached the solution up to the accuracy defined by
  // kRelativeConvergenceThreshold above (in this case,
  // minimizer.reached_solution == true and minimizer.converged() == true)
  // 2. The algorithm stopped improving as measured by the
  // kSimpleConvergenceCheck. In this case, minimizer.converged() == true
  // kLossEpochs and kConvergenceEpochs must be positive (and
  // probably both not larger than 10). Also, kLossEpochs <=
  // kConvergenceEpochs makes more sense.
  static const int kLossEpochs = 5;
  static const int kConvergenceMeasures = 10;
  // kMaxEpochs, kMaxNonzeroCoefficients, and kMaxSolution1Norm are global
  // stopping parameters for the lasso path.
  // kMaxEpochs denotes the limit on the total number of epochs (iterations)
  // run; the actual number can be up to two times larger (not including the
  // second RAPPOR step) because every individual lasso subproblem (run of
  // minimizer) has the same bound and the total epochs count is updated after
  // the subproblem is run.
  static const int kMaxEpochs = 20000;
  // kMaxNonzeroCoefficients is the maximum expected number of nonzero
  // coefficients in the solution. The final step of the lasso path will be
  // entered when this number of nonzeros (i.e. coefficients larger than
  // kZeroThreshold) have been identified in the algorithm; otherwise the
  // algorithm will perform all kNumLassoSteps (unless it reaches the maximum
  // number of epochs); notice that if you want to detect
  // candidates that account for p proportion of the observations, you will look
  // for at most 1/p candidates. For example, if you only care about candidates
  // that account for at least 1% of observations, you can set
  // kMaxNonzeroCoefficients = 100.
  static const int kMaxNonzeroCoefficients = 500;
  // We expect the real underlying solution to have 1-norm equal to 1.0, and
  // even more so, the solution of the penalized problem should have norm
  // smaller than 1.0. Thus, we also want to stop the lasso computations before
  // the 1-norm of the current solution vector (est_candidate_weights)
  // approaches 1.0. This is closely related to the standard representation of
  // the lasso problem. If we stop when the 1-norm equals 1.0, the lasso
  // solution also solves the quadratic program: min || A x - y ||_2 subject to
  // || x ||_2 <= 1.0. However, note that if there is an exact solution to Ax =
  // y with || x ||_2 = 1.0, it will not be found in the lasso step because of
  // penalty. It may be found in the second RAPPOR step where the penalty is
  // insignificant.
  static const double kMaxSolution1Norm = 0.9;
  // alpha is a constant from parallel boosting with momentum paper,
  // must be 0 < alpha < 1; lossmin library default initial choice is 0.5,
  // but we need to be able to reset this value in minimizer if needed.
  static const double alpha = 0.5;
  // kNumRunsSecondStep is the number of runs to estimate standard deviations of
  // coefficients in the second RAPPOR step (in GetSignificantNonZeros).
  static const int kNumRunsSecondStep = 20;
  // kMaxEpochsSingleRunSecondStep is the maximum number of epochs of a single
  // solve in the second RAPPOR step.
  static const int kMaxEpochsSingleRunSecondStep = 500;

  // Perform initializations based on the chosen parameters.
  // l1max is the smallest value such that the solution to the lasso problem is
  // zero. It is equal to the infinity norm of the unpenalized objective
  // gradient at zero.
  const uint32_t num_bits = config_->num_bits();
  const uint32_t num_cohorts = config_->num_cohorts();
  const uint32_t num_hashes = config_->num_hashes();
  const double l1max = initial_gradient.array().abs().maxCoeff();
  const double l1min = kL1maxToL1minRatio * l1max;
  const double l2 = kL2toL1Ratio * l1min;
  const double l1delta = (l1max - l1min) / kNumLassoSteps;
  const double initial_mean_gradient_norm =
      initial_gradient.norm() / num_candidates;
  const double kConvergenceThreshold = std::max(
      1e-12, kRelativeConvergenceThreshold * initial_mean_gradient_norm);
  const double kInLassoPathConvergenceThreshold =
      std::max(1e-12, kRelativeInLassoPathConvergenceThreshold *
                          initial_mean_gradient_norm);
  // In the first step identify at most 0.5 * M coefficients, where M is the
  // number of rows of the cadidate matrix (but no more than
  // kMaxNonzeroCoefficients). This is a heuristic to ensure that the matrix in
  // the second step is full column rank.
  // TODO(bazyli) possibly add more heuristics; including "bogus" candidates
  const int max_nonzero_coeffs = std::min(
      num_candidates, std::min(static_cast<int>(0.5 * num_cohorts * num_bits),
                               kMaxNonzeroCoefficients));
  int total_epochs_run = 0;
  minimizer.set_zero_threshold(kZeroThreshold);
  minimizer.set_convergence_threshold(kInLassoPathConvergenceThreshold);
  minimizer.set_simple_convergence_threshold(kSimpleConvergenceThreshold);
  minimizer.set_l2(l2);
  minimizer.compute_and_set_learning_rates();  // learning rates must be
                                               // re-computed when l2 penalty
                                               // changes.

  VLOG(4) << "Initial gradient norm == " << initial_mean_gradient_norm;
  VLOG(4) << "Convergence Threshold" << kConvergenceThreshold;

  // Run the first step of RAPPOR (perform the lasso path computations).
  // TODO(bazyli) a function?
  std::vector<double> loss_history;
  double solution_1_norm = 0;
  int how_many_nonzero_coeffs =
      0;  // Number of identified nonzero coefficients.
  int i = 0;
  for (; i < kNumLassoSteps + 1 && total_epochs_run < kMaxEpochs; i++) {
    minimizer.set_l1(l1max - i * l1delta);
    // Set minimizer input as in the parallel boosting with momentum paper.
    minimizer.set_phi_center(est_candidate_weights);
    minimizer.set_converged(false);
    minimizer.set_alpha(alpha);
    minimizer.set_beta(1 - alpha);

    // Compute the 1-norm of the current solution and the number of nonzero
    // coefficients.
    solution_1_norm = est_candidate_weights.lpNorm<1>();
    how_many_nonzero_coeffs =
        (est_candidate_weights.array() > kZeroThreshold).count();

    VLOG(4) << "Minimizing " << i
            << "-th subproblem, with l1 == " << l1max - i * l1delta;
    // TODO(bazyli) need to add a stopping rule if no new nonzeros have been
    // added for a long time?
    if (how_many_nonzero_coeffs >= max_nonzero_coeffs || i == kNumLassoSteps ||
        solution_1_norm > kMaxSolution1Norm) {
      // Enter the final lasso step
      minimizer.set_convergence_threshold(kConvergenceThreshold);
      i = kNumLassoSteps;
      VLOG(4) << "Entered last Run";
    }

    minimizer.Run(kMaxEpochs, kLossEpochs, kConvergenceMeasures,
                  &est_candidate_weights, &loss_history);
    VLOG(4) << "Ran" << minimizer.num_epochs_run() << "epochs in this step.";
    VLOG(4) << "Num of nonzero coefficients found: " << how_many_nonzero_coeffs;
    total_epochs_run += minimizer.num_epochs_run();
  }
  VLOG(4) << "Ran " << total_epochs_run << " epochs in total.";

  // We will need to construct the matrix for the second step. This matrix
  // is composed of columns corresponding to identified nonzero candidates.
  // how_many_nonzero_coeffs is an approximation of their number.
  std::vector<int> second_step_cols;
  second_step_cols.reserve(how_many_nonzero_coeffs);
  for (int i = 0; i < est_candidate_weights.size(); i++) {
    if (est_candidate_weights[i] > kZeroThreshold) {
      second_step_cols.push_back(i);
    }
  }

  // We will construct the matrix from triplets which is simple and efficent.
  const uint32_t second_step_num_candidates = second_step_cols.size();
  const uint32_t num_observations = bit_counter_.num_observations();
  const int nonzero_matrix_entries =
      num_cohorts * num_hashes * second_step_num_candidates;
  std::vector<Eigen::Triplet<double>> second_step_matrix_triplets;
  second_step_matrix_triplets.reserve(nonzero_matrix_entries);

  // Convert the candidate_matrix_ to colmajor format to iterate over columns
  // efficiently and form the triplets.
  Eigen::SparseMatrix<double, Eigen::ColMajor> candidate_matrix_col_major =
      candidate_matrix;
  for (uint32_t col_i = 0; col_i < second_step_num_candidates; col_i++) {
    // Iterate over column corresponding to this candidate and update
    // triplets.
    for (Eigen::SparseMatrix<double, Eigen::ColMajor>::InnerIterator it(
             candidate_matrix_col_major, second_step_cols[col_i]);
         it; ++it) {
      second_step_matrix_triplets.push_back(
          Eigen::Triplet<double>(it.row(), col_i, it.value()));
    }
  }

  // Build the second step matrix.
  Eigen::SparseMatrix<double, Eigen::RowMajor> candidate_submatrix_second_step(
      candidate_matrix_.rows(), second_step_num_candidates);
  candidate_submatrix_second_step.setFromTriplets(
      second_step_matrix_triplets.begin(), second_step_matrix_triplets.end());

  // Prepare initial guess for the second step of RAPPOR.
  lossmin::Weights est_candidate_weights_second_step =
      lossmin::Weights(second_step_num_candidates);
  for (uint32_t i = 0; i < second_step_num_candidates; i++) {
    int first_step_col_num = second_step_cols[i];
    est_candidate_weights_second_step[i] =
        est_candidate_weights[first_step_col_num];
  }

  // We can now run the second step of RAPPOR.
  // ************************************************************************
  // Note(bazyli) We will use parallel boosting with
  // momentum again, with very small l1 and l2 penalties. We could use a
  // standard least squares solver here (e.g. based on QR decomposition).
  // However, we still cannot technically guarantee that the columns are
  // linearly independent, and so a tiny penalty will prevent the coefficients
  // from behaving ''wildly'' (this is a better-safe-than-sorry choice); also,
  // we have a very good initial point from the lasso computations so we expect
  // to converge fast (and the problem is smaller than before). Conceptually, we
  // are just solving the least squares problem.
  //
  // The values from lasso computations are a bit biased (likely lower)
  // because of penalty, so the solution after this step should be closer to the
  // underlying distribution. Also, we want to compute standard errors.
  //
  // The RAPPOR paper performs least squares computation at this point; it has
  // closed-form expression for standard errors of coefficients (more precisely,
  // the covariane matrix), which we could use. However, this expression
  // involves the matrix (A_s^T A_s)^(-1), where A_s ==
  // candidate_submatrix_second_step; a priori the inverse might not exist
  // (or A_s^T A_s can be numerically singular or very ill-conditioned).
  // Although unlikely, we do not want to check that. Even if it is not the
  // case, computing the inverse might be computationally (too) heavy.
  // Therefore, we will simulate the behavior of the coefficients to approximate
  // the standard errors.
  //
  // The standard deviations in the linear regression problem min || A * x -
  // b||_2 are computed with the assumption that y_i = < a_i,x_i > + err_i,
  // where err_i are i.i.d. gaussian errors (so technically speaking, these
  // p-values are not entirely correct in our case). Although we do not compute
  // the theoretical standard errors, we nevertheless make the same assumption
  // that the noise of detecting y_i (in our case, this is the normalized count
  // of bit i), is Gaussian with standard deviation equal to the standard error
  // of y_i (stored in the est_std_errors). We assume these errors to be
  // independent. We thus simulate the standard errors of the coefficients by
  // introducing this noise directly in y (as_label_set) num_runs time, each
  // time re-running the minimizer with the same input (see the description of
  // GetExactValuesAndStdErrs for details).
  //
  // In any event, this process is conditioned on the choice made by the lasso
  // step (as are standarde errors from least squares used in RAPPOR). They do
  // not accout for anything random that happens in the first step above. Other
  // possible choices to get the p-values include: 1. bootstrap on the pairs
  // (a_i,y_i) -- but this could give zero columns. 2. p-values for the
  // particular choice of final penalty in lasso -- described in "Statistical
  // Learning with Sparsity: The Lasso and Generalizations" pp. 150 -- but
  // this probably would account only for the first step.
  // ************************************************************************

  // Initialize the parameters. We are using l1 penalty value that is 100 times
  // smaller than the final value used in the lasso path;
  const double l1_second_step = 1e-2 * minimizer.l1();
  const double l2_second_step = kL2toL1Ratio * l1_second_step;
  // Run the second step of RAPPOR to obtain better estimates of candidate
  // values, zeroing out insignificant ones.
  lossmin::Weights exact_candidate_weights_second_step(num_candidates);
  lossmin::Weights est_candidate_errors_second_step(num_candidates);
  GetExactValuesAndStdErrs(
      l1_second_step, l2_second_step, kNumRunsSecondStep,
      kMaxEpochsSingleRunSecondStep, kLossEpochs, kConvergenceMeasures,
      kZeroThreshold, est_candidate_weights_second_step, est_std_errors,
      candidate_submatrix_second_step, as_label_set,
      &exact_candidate_weights_second_step, &est_candidate_errors_second_step);

  // Prepare the final solution vector
  results_out->resize(num_candidates);
  for (int i = 0; i < num_candidates; i++) {
    results_out->at(i).count_estimate = 0;
    results_out->at(i).std_error = 0;
  }
  // Write the results from the second step of RAPPOR.
  for (uint32_t i = 0; i < second_step_num_candidates; i++) {
    int first_step_col_num = second_step_cols[i];
    results_out->at(first_step_col_num).count_estimate =
        exact_candidate_weights_second_step[i] * num_observations;
    results_out->at(first_step_col_num).std_error =
        est_candidate_errors_second_step[i] * num_observations;
  }

  // Save minimizer data afer run (from the first step of RAPPOR).
  minimizer_data_.num_epochs_run = total_epochs_run;
  minimizer_data_.converged = minimizer.converged();
  minimizer_data_.reached_solution = minimizer.reached_solution();
  if (!loss_history.empty()) {
    minimizer_data_.final_loss = loss_history.back();
  }
  minimizer_data_.l1 = minimizer.l1();
  minimizer_data_.l2 = minimizer.l2();
  minimizer_data_.convergence_threshold = kConvergenceThreshold;

  if (!minimizer.converged()) {
    std::string message = "The last lasso subproblem did not converge.";
    LOG_STACKDRIVER_COUNT_METRIC(ERROR, kAnalyzeFailure) << message;
    return grpc::Status(grpc::DEADLINE_EXCEEDED, message);
  }

  if (i != kNumLassoSteps + 1) {
    std::string message = "The lasso path did not reach the last subproblem.";
    LOG_STACKDRIVER_COUNT_METRIC(ERROR, kAnalyzeFailure) << message;
    return grpc::Status(grpc::DEADLINE_EXCEEDED, message);
  }

  return grpc::Status::OK;
}

grpc::Status RapporAnalyzer::ExtractEstimatedBitCountRatiosAndStdErrors(
    Eigen::VectorXd* est_bit_count_ratios,
    std::vector<double>* est_std_errors) {
  VLOG(5) << "RapporAnalyzer::ExtractEstimatedBitCountRatiosAndStdErrors()";
  CHECK(est_bit_count_ratios);

  if (!config_->valid()) {
    return grpc::Status(grpc::INVALID_ARGUMENT,
                        "Invalid RapporConfig passed to constructor.");
  }

  if (candidate_map_.candidate_list == nullptr ||
      candidate_map_.candidate_list->candidates_size() == 0) {
    return grpc::Status(grpc::INVALID_ARGUMENT,
                        "Cannot perform RAPPOR analysis because no candidate "
                        "list was specified.");
  }

  const uint32_t num_bits = config_->num_bits();
  const uint32_t num_cohorts = config_->num_cohorts();

  est_bit_count_ratios->resize(num_cohorts * num_bits);
  est_std_errors->resize(num_cohorts * num_bits);

  const std::vector<CohortCounts>& estimated_counts =
      bit_counter_.EstimateCounts();
  CHECK(estimated_counts.size() == num_cohorts);

  int cohort_block_base = 0;
  for (auto& cohort_data : estimated_counts) {
    CHECK(cohort_data.count_estimates.size() == num_bits);
    for (size_t bit_index = 0; bit_index < num_bits; bit_index++) {
      // |bit_index| is an index "from the right".
      size_t bloom_index = num_bits - 1 - bit_index;
      (*est_bit_count_ratios)(cohort_block_base + bloom_index) =
          cohort_data.count_estimates[bit_index] /
          static_cast<double>(cohort_data.num_observations);
      (*est_std_errors)[cohort_block_base + bloom_index] =
          cohort_data.std_errors[bit_index] /
          static_cast<double>(cohort_data.num_observations);
    }
    cohort_block_base += num_bits;
  }
  return grpc::Status::OK;
}

grpc::Status RapporAnalyzer::BuildCandidateMap() {
  VLOG(5) << "RapporAnalyzer::BuildCandidateMap()";
  if (!config_->valid()) {
    return grpc::Status(grpc::FAILED_PRECONDITION,
                        "Invalid RapporConfig passed to constructor.");
  }

  if (candidate_map_.candidate_list == nullptr ||
      candidate_map_.candidate_list->candidates_size() == 0) {
    return grpc::Status(grpc::INVALID_ARGUMENT,
                        "Cannot perform RAPPOR analysis because no candidate "
                        "list was specified.");
  }

  // TODO(rudominer) We should cache candidate_matrix_ rather than recomputing
  // candidate_map_ and candidate_matrix_ each time.

  const uint32_t num_bits = config_->num_bits();
  const uint32_t num_cohorts = config_->num_cohorts();
  const uint32_t num_hashes = config_->num_hashes();
  const uint32_t num_candidates =
      candidate_map_.candidate_list->candidates_size();

  if (VLOG_IS_ON(4)) {
    VLOG(4) << "RapporAnalyzer: Start list of " << num_candidates
            << " candidates:";
    for (const std::string& candidate :
         candidate_map_.candidate_list->candidates()) {
      VLOG(4) << "RapporAnalyzer: candidate: " << candidate;
    }
    VLOG(4) << "RapporAnalyzer: End list of " << num_candidates
            << " candidates.";
  }

  candidate_matrix_.resize(num_cohorts * num_bits, num_candidates);
  std::vector<Eigen::Triplet<double>> sparse_matrix_triplets;
  sparse_matrix_triplets.reserve(num_candidates * num_cohorts * num_hashes);

  int column = 0;
  for (const std::string& candidate :
       candidate_map_.candidate_list->candidates()) {
    // In rappor_encoder.cc it is not std::strings that are encoded but rather
    // |ValuePart|s. So here we want to take the candidate as a string and
    // convert it into a serialized |ValuePart|.
    ValuePart candidate_as_value_part;
    candidate_as_value_part.set_string_value(candidate);
    std::string serialized_candidate;
    candidate_as_value_part.SerializeToString(&serialized_candidate);

    // Append a CohortMap for this candidate.
    candidate_map_.candidate_cohort_maps.emplace_back();
    CohortMap& cohort_map = candidate_map_.candidate_cohort_maps.back();

    // Iterate through the cohorts.
    int row_block_base = 0;
    for (size_t cohort = 0; cohort < num_cohorts; cohort++) {
      // Append an instance of |Hashes| for this cohort.
      cohort_map.cohort_hashes.emplace_back();
      Hashes& hashes = cohort_map.cohort_hashes.back();

      // Form one big hashed value of the serialized_candidate. This will be
      // used to obtain multiple bit indices.
      byte hashed_value[crypto::hash::DIGEST_SIZE];
      if (!RapporEncoder::HashValueAndCohort(serialized_candidate, cohort,
                                             num_hashes, hashed_value)) {
        return grpc::Status(grpc::INTERNAL,
                            "Hash operation failed unexpectedly.");
      }

      // bloom_filter is indexed "from the left". That is bloom_filter[0]
      // corresponds to the most significant bit of the first byte of the
      // Bloom filter.
      std::vector<bool> bloom_filter(num_bits, false);

      // Extract one bit index for each of the hashes in the Bloom filter.
      for (size_t hash_index = 0; hash_index < num_hashes; hash_index++) {
        uint32_t bit_index =
            RapporEncoder::ExtractBitIndex(hashed_value, hash_index, num_bits);
        hashes.bit_indices.push_back(bit_index);
        // |bit_index| is an index "from the right".
        bloom_filter[num_bits - 1 - bit_index] = true;
      }

      // Add triplets to the sparse matrix representation. For the current
      // column and the current block of rows we add a 1 into the row
      // corresponding to the index of each set bit in the Bloom filter.
      for (size_t bloom_index = 0; bloom_index < num_bits; bloom_index++) {
        if (bloom_filter[bloom_index]) {
          int row = row_block_base + bloom_index;
          sparse_matrix_triplets.emplace_back(row, column, 1.0);
        }
      }

      // In our sparse matrix representation each cohort corresponds to a block
      // of |num_bits| rows.
      row_block_base += num_bits;
    }
    // In our sparse matrix representation a column corresponds to a candidate.
    column++;
    row_block_base = 0;
  }

  candidate_matrix_.setFromTriplets(sparse_matrix_triplets.begin(),
                                    sparse_matrix_triplets.end());

  return grpc::Status::OK;
}

void RapporAnalyzer::GetExactValuesAndStdErrs(
    const double l1, const double l2, const int num_runs, const int max_epochs,
    const int loss_epochs, const int convergence_epochs,
    const double zero_threshold, const lossmin::Weights& est_candidate_weights,
    const std::vector<double>& est_standard_errs,
    const lossmin::InstanceSet& instances,
    const lossmin::LabelSet& as_label_set,
    lossmin::Weights* exact_est_candidate_weights,
    lossmin::Weights* est_candidate_errors) {
  // Initialize the minimizer constants (these are analogous to the ones used in
  // Analyze).
  static const double alpha = 0.5;
  static const double kRelativeConvergenceThreshold = 1e-6;
  static const double kSimpleConvergenceThreshold = 1e-5;
  const int num_candidates = est_candidate_weights.size();
  const int num_labels = as_label_set.size();
  int num_converged = 0;

  // We will need the solutions from all runs to compute standard errors.
  std::vector<lossmin::Weights> est_weights_runs;
  lossmin::Weights mean_est_weights = lossmin::Weights::Zero(num_candidates);
  lossmin::LinearRegressionLossFunction loss_function;

  // In each run create a new_label_set by adding gaussian noise to the original
  // as_label_set.
  for (int i = 0; i < num_runs; i++) {
    lossmin::LabelSet new_label_set = as_label_set;

    for (int j = 0; j < num_labels; j++) {
      std::normal_distribution<double> nrm_distr(
          0, static_cast<double>(est_standard_errs[j]));
      double noise = nrm_distr(random_dev_);
      new_label_set(j) += noise;
    }

    // Run the minimizer for the right hand side with noise.
    // Each time use the est_candidate_weights as the initial guess.
    lossmin::Weights new_candidate_weights = est_candidate_weights;
    std::vector<double> loss_history_not_used;
    lossmin::GradientEvaluator grad_eval(instances, new_label_set,
                                         &loss_function);
    lossmin::ParallelBoostingWithMomentum minimizer(l1, l2, grad_eval);
    lossmin::Weights initial_gradient = lossmin::Weights(num_candidates);
    minimizer.SparseInnerProductGradient(new_candidate_weights,
                                         &initial_gradient);
    // Set minimizer input data.
    double initial_mean_gradient_norm =
        initial_gradient.norm() / num_candidates;
    double convergence_threshold = std::max(
        1e-12, kRelativeConvergenceThreshold * initial_mean_gradient_norm);
    minimizer.set_phi_center(new_candidate_weights);
    minimizer.set_convergence_threshold(convergence_threshold);
    minimizer.set_zero_threshold(zero_threshold);
    minimizer.set_simple_convergence_threshold(kSimpleConvergenceThreshold);
    minimizer.set_alpha(alpha);
    minimizer.set_beta(1 - alpha);
    minimizer.Run(max_epochs, loss_epochs, convergence_epochs,
                  &new_candidate_weights, &loss_history_not_used);
    if (minimizer.converged()) {
      // Update the mean and store the current solution.
      mean_est_weights += new_candidate_weights;
      est_weights_runs.push_back(new_candidate_weights);
      num_converged++;
    }
  }
  if (num_converged > 0) {
    mean_est_weights /= num_converged;
  } else {
    mean_est_weights = est_candidate_weights;
  }

  // Compute the sample means and standard deviations (standard errors).
  lossmin::Weights sample_stds = lossmin::Weights::Zero(num_candidates);
  if (num_converged >= 5) {
    for (auto& est_weight : est_weights_runs) {
      sample_stds += (est_weight - mean_est_weights).array().pow(2).matrix();
    }
    sample_stds = sample_stds / (num_converged - 1);
    sample_stds = sample_stds.array().sqrt();
  }
  *est_candidate_errors = sample_stds;
  *exact_est_candidate_weights = mean_est_weights;
}

}  // namespace rappor
}  // namespace cobalt

/*

Justification for the formula used in ExtractEstimatedBitCountRatiosAndStdErrors
-------------------------------------------------------------------
See the comments at the declaration of the method
ExtractEstimatedBitCountRatiosAndStdErrors() in rappor_analyzer.h for the
context and the definitions of the symbols used here.

Here we justify the use of the formula

     est_bit_count_ratios[i*k +j] = est_count_i_j / n_i.

Let A be the binary sparse matrix produced by the method BuildCandidateMap()
and stored in candidate_matrix_. Let b be the column vector produced by
the method ExtractEstimatedBitCountRatiosAndStdErrors() and stored in the
variable est_bit_count_ratios.  In RapporAnalyzer::Analyze() we compute an
estimate of a solution to the equation Ax = b. The question we want to address
here is how do we know we are using the correct value of b? In particular, why
is it appropriate to divide each entry by n_i, the number of observations from
cohort i?

The assumption that underlies the justifcation is that the probability of
a given candidate string occurring is the same in each cohort. That is, there
is a probability distribution vector x_0 of length s = # of candidates such
that for each cohort i < m, and each candidate index r < s,
x_0[r] =
   (number of true observations of candidate r in cohort i) /
        (number of observations from cohort i)

Assume such an x_0 exists. Now let n_i = (number of observations from cohort i).
Then consider the vector b_i = A (n_i) x_0. We are only concerned with the
entries in b_i corresponding to cohort i, that is the entries
i*k + j for 0 <= j < k. Fix such a j and note that
b_i[i*k + j] = "the true count of 1's for bit j in cohort i". That is, the
count of 1's for bit j in cohort i prior to flipping bits for randomized
response. In other words, the count of 1's if we use p = 0, q = 1.

Dividing both sides of the equation A (n_i) x_0 = b_i by n_i and focusing
only on cohort i we get
     A x_0 [i*k + j] = "the true count of 1's for bit j in cohort i" / n_i

Let b* = A x_0. Then we have:

(i) x_0 is a solution to the equation Ax = b*
(ii) b*[i*k + j] = "the true count of 1's for bit j in cohort i" / n_i

This justifies our use of the vector b. We have
 b[i*k + j] = "the estimated count of 1's for bit j in cohort i" / n_i

 and we seek an estimate to an x such that Ax = b. Such an x may therefore
 naturally be considered to be an estimate of x_0.

*/
