// Copyright 2014 Google Inc. All Rights Reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_UTIL_LOSSMIN_EIGEN_TYPES_H_
#define COBALT_UTIL_LOSSMIN_EIGEN_TYPES_H_

#include "third_party/eigen/Eigen/Core"
#include "third_party/eigen/Eigen/SparseCore"

namespace cobalt_lossmin {

// Arrays.
typedef Eigen::Array<bool, Eigen::Dynamic, 1> ArrayXb;
typedef Eigen::ArrayXf ArrayXf;
typedef Eigen::ArrayXd ArrayXd;
typedef Eigen::ArrayXi ArrayXi;

// Vectors.
typedef Eigen::VectorXf VectorXf;
typedef Eigen::VectorXd VectorXd;
typedef Eigen::VectorXi VectorXi;

// Sparse Vectors.
typedef Eigen::SparseVector<float> SparseVectorXf;
typedef Eigen::SparseVector<double> SparseVectorXd;

// Matrix.
typedef Eigen::Matrix<
    float,
    Eigen::Dynamic,
    Eigen::Dynamic,
    Eigen::ColMajor> MatrixXf;
typedef Eigen::Matrix<
    float,
    Eigen::Dynamic,
    Eigen::Dynamic,
    Eigen::RowMajor> RowMatrixXf;
typedef Eigen::Matrix<
    double,
    Eigen::Dynamic,
    Eigen::Dynamic,
    Eigen::ColMajor> MatrixXd;
typedef Eigen::Matrix<
    double,
    Eigen::Dynamic,
    Eigen::Dynamic,
    Eigen::RowMajor> RowMatrixXd;

// Instances and parameters.
typedef VectorXd Weights;

typedef VectorXd Label;
typedef RowMatrixXd LabelSet;

typedef Eigen::SparseVector<double> Instance;
typedef Eigen::SparseMatrix<double, Eigen::RowMajor> InstanceSet;
typedef Eigen::SparseMatrix<double, Eigen::ColMajor> SparseMatrixXd;

}  // namespace cobalt_lossmin

#endif  // COBALT_UTIL_LOSSMIN_EIGEN_TYPES_H_
