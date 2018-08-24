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

#ifndef NETKET_OBSERVABLE_HPP
#define NETKET_OBSERVABLE_HPP

#include <string>
#include <vector>

#include "Hamiltonian/local_operator.hpp"
#include "Hilbert/hilbert.hpp"
#include "Utils/json_helper.hpp"

#include "abstract_observable.hpp"
#include "custom_observable.hpp"

namespace netket {

class Observable : public AbstractObservable {
  using Ptype = std::unique_ptr<AbstractObservable>;
  Ptype o_;

 public:
  using MatType = LocalOperator::MatType;

  Observable(const Hilbert &hilbert, const json &obspars) {
    CheckFieldExists(obspars, "Operators", "Observables");
    CheckFieldExists(obspars, "ActingOn", "Observables");
    CheckFieldExists(obspars, "Name", "Observables");

    auto jop = obspars.at("Operators").get<std::vector<MatType>>();
    auto sites = obspars.at("ActingOn").get<std::vector<std::vector<int>>>();
    std::string name = obspars.at("Name");

    o_ = Ptype(new CustomObservable(hilbert, jop, sites, name));
  }

  static std::vector<Observable> FromJson(const Hilbert &hilbert,
                                          const json &pars) {
    std::vector<Observable> observables;

    if (FieldExists(pars, "Observables")) {
      auto obspar = pars["Observables"];

      if (obspar.is_array()) {
        // multiple observables case
        for (std::size_t i = 0; i < obspar.size(); i++) {
          observables.emplace_back(hilbert, obspar[i]);
        }
      } else {
        // single observable case
        observables.emplace_back(hilbert, obspar);
      }
    }
    return observables;
  }

  void FindConn(const Eigen::VectorXd &v,
                std::vector<std::complex<double>> &mel,
                std::vector<std::vector<int>> &connectors,
                std::vector<std::vector<double>> &newconfs) const override {
    return o_->FindConn(v, mel, connectors, newconfs);
  }

  const Hilbert &GetHilbert() const override { return o_->GetHilbert(); }

  const std::string Name() const override { return o_->Name(); }
};

}  // namespace netket

#endif
