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

#ifndef NETKET_GROUND_STATE_HPP
#define NETKET_GROUND_STATE_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Hamiltonian/MatrixWrapper/matrix_wrapper.hpp"
#include "Observable/observable.hpp"
#include "Optimizer/optimizer.hpp"

#include "exact_diagonalization.hpp"
#include "imaginary_time.hpp"
#include "variational_exact.hpp"
#include "variational_montecarlo.hpp"

#include "Hilbert/hilbert_index.hpp"

namespace netket {

class GroundState {
 public:
  explicit GroundState(const json& pars) {
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

    } else if (method_name == "GdExact" || method_name == "SrExact") {
      using MachineType = Machine<std::complex<double>>;
      MachineType machine(graph, hamiltonian, pars);

      Sampler<MachineType> sampler(graph, hamiltonian, machine, pars);
      Optimizer optimizer(pars);

      VariationalExact<MachineType> vmc(hamiltonian, sampler, optimizer, pars);
      vmc.Run();

    } else if (method_name == "Lanczos") {
      using MachineType = Lanczos<std::complex<double>>;
      MachineType machine(graph, hamiltonian, pars);

      Sampler<MachineType> sampler(graph, hamiltonian, machine, pars);
      Optimizer optimizer(pars);

      VariationalMonteCarlo<MachineType> vmc(hamiltonian, sampler, optimizer,
                                             pars);
      vmc.Run();

    } else if (method_name == "Sum") {
      using MachineType = PsiSum<std::complex<double>>;
      MachineType machine(graph, hamiltonian, pars);

      Sampler<MachineType> sampler(graph, hamiltonian, machine, pars);
      Optimizer optimizer(pars);

      VariationalMonteCarlo<MachineType> vmc(hamiltonian, sampler, optimizer,
                                             pars);
      vmc.Run();

    } else if (method_name == "SumExact") {
      using MachineType = PsiSum<std::complex<double>>;
      MachineType machine(graph, hamiltonian, pars);

      Sampler<MachineType> sampler(graph, hamiltonian, machine, pars);
      Optimizer optimizer(pars);

      VariationalExact<MachineType> vmc(hamiltonian, sampler, optimizer, pars);
      vmc.Run();

    } else if (method_name == "ImaginaryTimePropagation") {
      int size;
      MPI_Comm_size(MPI_COMM_WORLD, &size);
      if (size > 1) {
        throw InvalidInputError(
            "Imaginary time propagation method currently only supports a "
            "single MPI process.");
      }

      auto observables = Observable::FromJson(hamiltonian.GetHilbert(), pars);

      const auto pars_gs = FieldVal(pars, "GroundState");
      auto driver =
          ImaginaryTimePropagation::FromJson(hamiltonian, observables, pars_gs);

      // Start with random initial vector
      Eigen::VectorXcd initial =
          Eigen::VectorXcd::Random(driver.GetDimension());
      initial.normalize();

      driver.Run(initial);

    } else if (method_name == "ED") {
      double precision;
      int n_eigenvalues, max_iter, random_seed;
      get_ed_parameters(pars, precision, n_eigenvalues, random_seed, max_iter);

      // Compute eigenvalues and groundstate, if needed
      eddetail::result_t edresult;
      std::string matrix_format = FieldOrDefaultVal<json, std::string>(
          pars["GroundState"], "MatrixFormat", "Sparse");
      bool get_groundstate = FieldExists(pars, "Observables");

      if (matrix_format == "Sparse") {
        edresult = lanczos_ed(hamiltonian, false, n_eigenvalues, max_iter,
                              random_seed, precision, get_groundstate);
      } else if (matrix_format == "Direct") {
        edresult = lanczos_ed(hamiltonian, true, n_eigenvalues, max_iter,
                              random_seed, precision, get_groundstate);
      } else if (matrix_format == "Dense") {
        edresult = full_ed(hamiltonian, n_eigenvalues, get_groundstate);
      } else {
        std::stringstream s;
        s << "Unknown MatrixFormat for ED: "
          << FieldVal(pars["GroundState"], "MatrixFormat");
        throw InvalidInputError(s.str());
      }

      // Evaluate observables
      std::map<std::string, double> observables_results;
      if (FieldExists(pars, "Observables")) {
        json j;
        j["MatrixWrapper"] = matrix_format;
        auto observables = Observable::FromJson(hamiltonian.GetHilbert(), pars);
        const auto& state = edresult.eigenvectors[0];

        // const auto& hilbert = hamiltonian.GetHilbert();
        // const HilbertIndex hilbert_index(hilbert);
        //
        // Eigen::VectorXd v1(16);
        // v1 << 1, 1, 1, 1, -1, -1, -1, -1, 1, 1, 1, 1, -1, -1, -1, -1;
        // Eigen::VectorXd v2(16);
        // v2 << 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1;
        //
        // auto i1 = hilbert_index.StateToNumber(v1);
        // auto i2 = hilbert_index.StateToNumber(v2);
        // std::cout << "i1 = " << state(i1) << std::endl;
        // std::cout << "i2 = " << state(i2) << std::endl;
        for (const auto& entry : observables) {
          const auto& obs = ConstructMatrixWrapper(j, entry);
          const auto value = obs->Mean(state).real();
          observables_results[entry.Name()] = value;
        }
      }

      write_ed_results(pars, edresult.eigenvalues, observables_results);

    } else {
      std::stringstream s;
      s << "Unknown GroundState method: " << method_name;
      throw InvalidInputError(s.str());
    }
  }
};

}  // namespace netket

#endif
