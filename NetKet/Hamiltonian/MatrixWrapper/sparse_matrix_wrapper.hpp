// Copyright 2018 Damian Hofmann - All Rights Reserved.
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

#ifndef NETKET_SPARSE_HAMILTONIAN_OPERATOR_HH
#define NETKET_SPARSE_HAMILTONIAN_OPERATOR_HH

#include <Eigen/SparseCore>

#include "Hilbert/hilbert_index.hpp"
#include "abstract_matrix_wrapper.hpp"

namespace netket {

/**
 * This class stores the matrix elements of a given Operator
 * (AbstractHamiltonian or AbstractObservable) as an Eigen dense matrix.
 */
template <class Operator, class WfType = Eigen::VectorXcd>
class SparseMatrixWrapper : public AbstractMatrixWrapper<Operator, WfType> {
  using Matrix = Eigen::SparseMatrix<std::complex<double>>;

  Matrix matrix_;
  int dim_;

 public:
  explicit SparseMatrixWrapper(const Operator& the_operator) {
    InitializeMatrix(the_operator);
  }

  WfType Apply(const WfType& state) const override { return matrix_ * state; }

  std::complex<double> Mean(const WfType& state) const override {
    return state.adjoint() * matrix_ * state;
  }

  std::array<std::complex<double>, 2> MeanVariance(
      const WfType& state) const override {
    auto state1 = matrix_ * state;
    auto state2 = matrix_ * state1;

    const std::complex<double> mean = state.adjoint() * state1;
    const std::complex<double> var = state.adjoint() * state2;

    return {{mean, var - std::pow(mean, 2)}};
  }

  int GetDimension() const override { return dim_; }

  const Matrix& GetMatrix() const { return matrix_; }

  /**
   * Computes the eigendecomposition of the given matrix.
   * @param options The options are passed directly to the constructor of
   * SelfAdjointEigenSolver.
   * @return An instance of Eigen::SelfAdjointEigenSolver initialized with the
   * wrapped operator and options.
   */
  Eigen::SelfAdjointEigenSolver<Matrix> ComputeEigendecomposition(
      int options = Eigen::ComputeEigenvectors) const {
    return Eigen::SelfAdjointEigenSolver<Matrix>(matrix_, options);
  }

 private:
  void InitializeMatrix(const Operator& the_operator) {
    const auto& hilbert = the_operator.GetHilbert();
    const HilbertIndex hilbert_index(hilbert);
    dim_ = hilbert_index.NStates();

    using Triplet = Eigen::Triplet<std::complex<double>>;
    
    std::vector<Triplet> tripletList;
    tripletList.reserve(dim_);

    matrix_.resize(dim_, dim_);
    matrix_.setZero();

    for (int i = 0; i < dim_; ++i) {
      auto v = hilbert_index.NumberToState(i);

      std::vector<std::complex<double>> matrix_elements;
      std::vector<std::vector<int>> connectors;
      std::vector<std::vector<double>> newconfs;
      the_operator.FindConn(v, matrix_elements, connectors, newconfs);

      for (size_t k = 0; k < connectors.size(); ++k) {
        auto vk = v;
        hilbert.UpdateConf(vk, connectors[k], newconfs[k]);
        auto j = hilbert_index.StateToNumber(vk);
        tripletList.push_back(Triplet(i, j, matrix_elements[k]));
      }
    }
    matrix_.setFromTriplets(tripletList.begin(), tripletList.end());
    matrix_.makeCompressed();
  }
};

}  // namespace netket

#endif  // NETKET_SPARSE_HAMILTONIAN_OPERATOR_HH
