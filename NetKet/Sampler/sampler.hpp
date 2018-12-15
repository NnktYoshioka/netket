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

#ifndef NETKET_SAMPLER_HPP
#define NETKET_SAMPLER_HPP

#include <memory>
#include <set>
#include "Graph/graph.hpp"
#include "Hamiltonian/hamiltonian.hpp"
#include "Utils/parallel_utils.hpp"
#include "abstract_sampler.hpp"
#include "custom_sampler.hpp"
#include "custom_sampler_pt.hpp"
#include "exact_sampler.hpp"
#include "exact_sz_conserved_sampler.hpp"
#include "metropolis_exchange.hpp"
#include "metropolis_exchange_pt.hpp"
#include "metropolis_global.hpp"
#include "metropolis_hamiltonian.hpp"
#include "metropolis_hamiltonian_pt.hpp"
#include "metropolis_hop.hpp"
#include "metropolis_local.hpp"
#include "metropolis_local_pt.hpp"

namespace netket {

template <class WfType>
class Sampler : public AbstractSampler<WfType> {
  using Ptype = std::unique_ptr<AbstractSampler<WfType>>;
  Ptype s_;

 public:
  explicit Sampler(WfType &psi, const json &pars) {
    CheckInput(pars);
    Init(psi, pars);
  }

  explicit Sampler(Graph &graph, WfType &psi, const json &pars) {
    CheckInput(pars);
    Init(psi, pars);
    Init(graph, psi, pars);
  }

  explicit Sampler(Hamiltonian &hamiltonian, WfType &psi, const json &pars) {
    CheckInput(pars);
    Init(psi, pars);
    Init(hamiltonian, psi, pars);
  }

  explicit Sampler(Graph &graph, Hamiltonian &hamiltonian, WfType &psi,
                   const json &pars) {
    CheckInput(pars);
    Init(psi, pars);
    Init(graph, psi, pars);
    Init(hamiltonian, psi, pars);
  }

  void Init(WfType &psi, const json &pars) {
    if (FieldExists(pars["Sampler"], "Name")) {
      if (pars["Sampler"]["Name"] == "MetropolisLocal") {
        s_ = Ptype(new MetropolisLocal<WfType>(psi));
      } else if (pars["Sampler"]["Name"] == "MetropolisLocalPt") {
        s_ = Ptype(new MetropolisLocalPt<WfType>(psi, pars));
      } else if (pars["Sampler"]["Name"] == "Exact") {
        s_ = Ptype(new ExactSampler<WfType>(psi));
      } else if (pars["Sampler"]["Name"] == "ExactSz") {
        s_ = Ptype(new ExactSzSampler<WfType>(psi));
      }
    } else {
      if (FieldExists(pars["Sampler"], "Nreplicas")) {
        s_ = Ptype(new CustomSamplerPt<WfType>(psi, pars));
      } else {
        s_ = Ptype(new CustomSampler<WfType>(psi, pars));
      }
    }
  }

  void Init(Graph &graph, WfType &psi, const json &pars) {
    if (FieldExists(pars["Sampler"], "Name")) {
      if (pars["Sampler"]["Name"] == "MetropolisExchange") {
        s_ = Ptype(new MetropolisExchange<WfType>(graph, psi, pars));
      } else if (pars["Sampler"]["Name"] == "MetropolisExchangePt") {
        s_ = Ptype(new MetropolisExchangePt<WfType>(graph, psi, pars));
      } else if (pars["Sampler"]["Name"] == "MetropolisHop") {
        s_ = Ptype(new MetropolisHop<WfType>(graph, psi, pars));
      } else if (pars["Sampler"]["Name"] == "MetropolisGlobal") {
        s_ = Ptype(new MetropolisGlobal<WfType>(graph, psi, pars));
      }
    }
  }

  void Init(Hamiltonian &hamiltonian, WfType &psi, const json &pars) {
    if (FieldExists(pars["Sampler"], "Name")) {
      if (pars["Sampler"]["Name"] == "MetropolisHamiltonian") {
        s_ = Ptype(
            new MetropolisHamiltonian<WfType, Hamiltonian>(psi, hamiltonian));
      } else if (pars["Sampler"]["Name"] == "MetropolisHamiltonianPt") {
        s_ = Ptype(new MetropolisHamiltonianPt<WfType, Hamiltonian>(
            psi, hamiltonian, pars));
      }
    }
  }

  void CheckInput(const json &pars) {
    int mynode;
    MPI_Comm_rank(MPI_COMM_WORLD, &mynode);

    CheckFieldExists(pars, "Sampler");
    if (FieldExists(pars["Sampler"], "Name")) {
      std::set<std::string> samplers = {
          "MetropolisLocal",       "MetropolisLocalPt",
          "MetropolisExchange",    "MetropolisExchangePt",
          "MetropolisHamiltonian", "MetropolisHamiltonianPt",
          "MetropolisHop",         "Exact",
          "MetropolisGlobal",      "ExactSz"};

      const auto name = pars["Sampler"]["Name"];

      if (samplers.count(name) == 0) {
        std::stringstream s;
        s << "Unknown Sampler.Name: " << name;
        throw InvalidInputError(s.str());
      }
    } else {
      if (!FieldExists(pars["Sampler"], "ActingOn") and
          !FieldExists(pars["Sampler"], "MoveOperators")) {
        throw InvalidInputError(
            "No SamplerName provided or Custom Sampler (MoveOperators and "
            "ActingOn) defined");
      }
    }
  }

  void Reset(bool initrandom = false) override { return s_->Reset(initrandom); }

  void Sweep() override { return s_->Sweep(); }

  Eigen::VectorXd Visible() override { return s_->Visible(); }

  void SetVisible(const Eigen::VectorXd &v) override {
    return s_->SetVisible(v);
  }

  WfType &Psi() override { return s_->Psi(); }

  Eigen::VectorXd Acceptance() const override { return s_->Acceptance(); }
};
}  // namespace netket

#endif
