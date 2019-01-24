
#include <fstream>
#include <string>
#include <vector>
#include "Utils/json_utils.hpp"

std::vector<netket::json> GetMachineInputs() {
  std::vector<netket::json> input_tests;
  netket::json pars;

  // Ising 1d

  pars = {{"Graph",
           {{"Name", "Hypercube"}, {"L", 20}, {"Dimension", 1}, {"Pbc", true}}},
          {"Machine", {{"Name", "RbmSpin"}, {"Alpha", 1.0}}},
          {"Hamiltonian", {{"Name", "Ising"}, {"h", 1.0}}}};
  input_tests.push_back(pars);

  // Heisenberg 1d
  pars = {{"Graph",
           {{"Name", "Hypercube"}, {"L", 20}, {"Dimension", 1}, {"Pbc", true}}},
          {"Machine", {{"Name", "RbmSpinSymm"}, {"Alpha", 2.0}}},
          {"Hamiltonian", {{"Name", "Heisenberg"}}}};
  input_tests.push_back(pars);

  // Heisenberg 1d with real fully connected FFNN
  pars = {{"Graph",
           {{"Name", "Hypercube"}, {"L", 4}, {"Dimension", 1}, {"Pbc", true}}},
          {"Machine",
           {{"Name", "FFNN"},
            {"Layers",
             {{{"Name", "RealInputFullyConnected"},
               {"Inputs", 4},
               {"Outputs", 8},
               {"Activation", "Lncosh"}},
              {{"Name", "RealSum"}, {"Inputs", 8}}}}}},
          {"Hamiltonian", {{"Name", "Heisenberg"}}}};
  input_tests.push_back(pars);

  // Bose-Hubbard 1d with symmetric machine
  pars = {{"Graph",
           {{"Name", "Hypercube"}, {"L", 20}, {"Dimension", 1}, {"Pbc", true}}},
          {"Machine", {{"Name", "RbmSpinSymm"}, {"Alpha", 1.0}}},
          {"Hamiltonian", {{"Name", "BoseHubbard"}, {"U", 4.0}, {"Nmax", 4}}}};
  input_tests.push_back(pars);

  // Bose-Hubbard 1d with non-symmetric rbm machine
  pars = {{"Graph",
           {{"Name", "Hypercube"}, {"L", 20}, {"Dimension", 1}, {"Pbc", true}}},
          {"Machine", {{"Name", "RbmSpin"}, {"Alpha", 2.0}}},
          {"Hamiltonian", {{"Name", "BoseHubbard"}, {"U", 4.0}, {"Nmax", 4}}}};
  input_tests.push_back(pars);

  // Bose-Hubbard 1d with multi-val rbm
  pars = {{"Graph",
           {{"Name", "Hypercube"}, {"L", 10}, {"Dimension", 1}, {"Pbc", true}}},
          {"Machine", {{"Name", "RbmMultival"}, {"Alpha", 2.0}}},
          {"Hamiltonian", {{"Name", "BoseHubbard"}, {"U", 4.0}, {"Nmax", 3}}}};
  input_tests.push_back(pars);

  // Ising 1d with jastrow
  pars = {{"Graph",
           {{"Name", "Hypercube"}, {"L", 20}, {"Dimension", 1}, {"Pbc", true}}},
          {"Machine", {{"Name", "Jastrow"}}},
          {"Hamiltonian", {{"Name", "Ising"}, {"h", 2.0}}}};
  input_tests.push_back(pars);

  // Heisemberg 1d with symmetric jastrow
  pars = {{"Graph",
           {{"Name", "Hypercube"}, {"L", 20}, {"Dimension", 1}, {"Pbc", true}}},
          {"Machine", {{"Name", "JastrowSymm"}}},
          {"Hamiltonian", {{"Name", "Heisenberg"}}}};
  input_tests.push_back(pars);

  // Bose-Hubbard 1d with non-symmetric Jastrow machine
  pars = {{"Graph",
           {{"Name", "Hypercube"}, {"L", 20}, {"Dimension", 1}, {"Pbc", true}}},
          {"Machine", {{"Name", "Jastrow"}}},
          {"Hamiltonian", {{"Name", "BoseHubbard"}, {"U", 4.0}, {"Nmax", 4}}}};
  input_tests.push_back(pars);

  // Bose-Hubbard 1d with symmetric Jastrow machine
  pars = {{"Graph",
           {{"Name", "Hypercube"}, {"L", 40}, {"Dimension", 1}, {"Pbc", true}}},
          {"Machine", {{"Name", "JastrowSymm"}}},
          {"Hamiltonian", {{"Name", "BoseHubbard"}, {"U", 4.0}, {"Nmax", 4}}}};
  input_tests.push_back(pars);

  return input_tests;
}
