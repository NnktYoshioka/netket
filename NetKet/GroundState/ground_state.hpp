// Copyright 2018 The Simons Foundation, Inc. - All Rights Reserved.
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

#ifndef NETKET_GROUND_STATE_CC
#define NETKET_GROUND_STATE_CC

#include <memory>

#include "Hamiltonian/MatrixWrapper/matrix_wrapper.hpp"
#include "Observable/observable.hpp"
#include "Optimizer/optimizer.hpp"

#include "imaginary_time.hpp"
#include "variational_montecarlo.hpp"

namespace netket {

class GroundState {
 public:
  explicit GroundState(const json &pars) {
    std::string method_name;

    if (FieldExists(pars, "GroundState")) {
      method_name = FieldVal(pars["GroundState"], "Method", "GroundState");
    } else if (FieldExists(pars, "Learning")) {
      method_name = FieldVal(pars["Learning"], "Method", "Learning");
      // DEPRECATED (to remove for v2.0.0)
      WarningMessage()
          << "Use of the Learning section is "
             "deprecated.\n Please use the dedicated GroundState section.\n";
    } else {
      std::stringstream s;
      s << "The GroundState section has not been specified.\n";
      throw InvalidInputError(s.str());
    }

    Graph graph(pars);
    Hamiltonian hamiltonian(graph, pars);

    if (method_name == "Gd" || method_name == "Sr") {
      using MachineType = Machine<std::complex<double>>;
      MachineType machine(graph, hamiltonian, pars);

      Sampler<MachineType> sampler(graph, hamiltonian, machine, pars);
      Optimizer optimizer(pars);

      VariationalMonteCarlo<MachineType> vmc(hamiltonian, sampler, optimizer,
                                             pars);
      vmc.Run();

    } else if (method_name == "Lanczos") {
      using MachineType = Lanczos<std::complex<double>>;
      MachineType machine(graph, hamiltonian, pars);

      Sampler<MachineType> sampler(graph, hamiltonian, machine, pars);
      Optimizer optimizer(pars);

      VariationalMonteCarlo<MachineType> vmc(hamiltonian, sampler, optimizer,
                                             pars);
      vmc.Run();

    } else if (method_name == "ImaginaryTimePropagation") {
      auto observables = Observable::FromJson(hamiltonian.GetHilbert(), pars);

      const auto pars_gs = FieldVal(pars, "GroundState");
      auto driver =
          ImaginaryTimePropagation::FromJson(hamiltonian, observables, pars_gs);

      // Start with random initial vector
      Eigen::VectorXcd initial =
          Eigen::VectorXcd::Random(driver.GetDimension());
      initial.normalize();

      driver.Run(initial);

    } else if (method_name == "Ed") {
      std::string file_base = FieldVal(pars["Learning"], "OutputFile");
      SaveEigenValues(hamiltonian, file_base + std::string(".log"));

    } else {
      std::stringstream s;
      s << "Unknown GroundState method: " << method_name;
      throw InvalidInputError(s.str());
    }
  }

  void SaveEigenValues(const Hamiltonian &hamiltonian,
                       const std::string &filename, int first_n = 1) {
    std::ofstream file_ed(filename);

    auto matrix = SparseMatrixWrapper<Hamiltonian>(hamiltonian);

    auto ed = matrix.ComputeEigendecomposition(Eigen::EigenvaluesOnly);

    auto eigs = ed.eigenvalues();
    eigs.conservativeResize(first_n);

    json j(eigs);
    file_ed << j << std::endl;

    file_ed.close();
  }
};

}  // namespace netket

#endif
