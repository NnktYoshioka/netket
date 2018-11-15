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

#ifndef NETKET_METROPOLISGLOBAL_HPP
#define NETKET_METROPOLISGLOBAL_HPP

#include <mpi.h>
#include <Eigen/Dense>
#include <cmath>
#include <iostream>
#include "Utils/parallel_utils.hpp"
#include "Utils/random_utils.hpp"
#include "abstract_sampler.hpp"

namespace netket {

// Metropolis sampling generating local exchanges
template <class WfType>
class MetropolisGlobal : public AbstractSampler<WfType> {
  WfType &psi_;

  const Hilbert &hilbert_;

  // number of visible units
  const int nv_;

  // length
  int L_;
  netket::default_random_engine rgen_;

  // states of visible units
  Eigen::VectorXd v_;

  Eigen::VectorXd accept_;
  Eigen::VectorXd moves_;

  int mynode_;
  int totalnodes_;

  // clusters to do updates
  std::vector<std::vector<int>> clusters_;

  // Look-up tables
  typename WfType::LookupType lt_;

 public:
  template <class G>
  MetropolisGlobal(G &graph, WfType &psi, int dmax = 1)
      : psi_(psi), hilbert_(psi.GetHilbert()), nv_(hilbert_.Size()) {
    Init(graph, dmax);
  }

  // Json constructor
  MetropolisGlobal(Graph &graph, WfType &psi, const json &pars)
      : psi_(psi), hilbert_(psi.GetHilbert()), nv_(hilbert_.Size()) {
    int dmax = FieldOrDefaultVal(pars["Sampler"], "Dmax", 1);
    Init(graph, dmax);
  }

  template <class G>
  void Init(G &graph, int dmax) {
    v_.resize(nv_);
    L_ = int(std::sqrt(nv_) + 0.5);

    MPI_Comm_size(MPI_COMM_WORLD, &totalnodes_);
    MPI_Comm_rank(MPI_COMM_WORLD, &mynode_);

    accept_.resize(2);
    moves_.resize(2);

    GenerateClusters(graph, dmax);

    Seed();

    Reset(true);

    InfoMessage() << "Metropolis Exchange with global moves sampler is ready "
                  << std::endl;
    InfoMessage() << dmax << " is the maximum distance for exchanges"
                  << std::endl;
  }

  template <class G>
  void GenerateClusters(G &graph, int dmax) {
    auto dist = graph.AllDistances();

    assert(int(dist.size()) == nv_);

    for (int i = 0; i < nv_; i++) {
      for (int j = 0; j < nv_; j++) {
        if (dist[i][j] <= dmax && i != j) {
          clusters_.push_back({i, j});
        }
      }
    }
  }

  void Seed(int baseseed = 0) {
    std::random_device rd;
    std::vector<int> seeds(totalnodes_);

    if (mynode_ == 0) {
      for (int i = 0; i < totalnodes_; i++) {
        seeds[i] = rd() + baseseed;
      }
    }

    SendToAll(seeds);

    rgen_.seed(seeds[mynode_]);
  }

  void Reset(bool initrandom = false) override {
    if (initrandom) {
      if (initrandom) {
        hilbert_.RandomVals(v_, rgen_);
      }
    }

    psi_.InitLookup(v_, lt_);

    accept_ = Eigen::VectorXd::Zero(2);
    moves_ = Eigen::VectorXd::Zero(2);
  }

  void Sweep() override {
    std::vector<int> tochange;
    std::uniform_real_distribution<double> distu;
    std::uniform_int_distribution<int> distcl(0, clusters_.size() - 1);
    std::uniform_int_distribution<int> distline(0, L_ - 1);

    std::vector<double> newconf;

    for (int i = 0; i < nv_; i++) {
      if (distu(rgen_) > 0.2) {
        tochange.resize(2);
        newconf.resize(2);
        int rcl = distcl(rgen_);
        assert(rcl < int(clusters_.size()));
        int si = clusters_[rcl][0];
        int sj = clusters_[rcl][1];

        assert(si < nv_ && sj < nv_);

        if (std::abs(v_(si) - v_(sj)) >
            std::numeric_limits<double>::epsilon()) {
          tochange = clusters_[rcl];
          newconf[0] = v_(sj);
          newconf[1] = v_(si);

          double ratio =
              std::norm(std::exp(psi_.LogValDiff(v_, tochange, newconf, lt_)));

          if (ratio > distu(rgen_)) {
            accept_[0] += 1;
            psi_.UpdateLookup(v_, tochange, newconf, lt_);
            hilbert_.UpdateConf(v_, tochange, newconf);
          }
        }
        moves_[0] += 1;
      } else {
        tochange.resize(0);
        newconf.resize(0);
        Eigen::Map<Eigen::MatrixXd> v_reshape(v_.data(), L_, L_);
        int r = distline(rgen_);
        if (distu(rgen_) > 0.5) {
          // swap col
          for (int j = 0; j < L_; j++) {
            if ((v_reshape.col(r)(j) - v_reshape.col((r + 1) % L_)(j)) >
                std::numeric_limits<double>::epsilon()) {
              tochange.push_back(r * L_ + j);
              tochange.push_back(((r + 1) % L_) * L_ + j);
              newconf.push_back(v_reshape.col((r + 1) % L_)(j));
              newconf.push_back(v_reshape.col(r)(j));
            }
          }
          if (tochange.size() != 0) {
            double ratio = std::norm(
                std::exp(psi_.LogValDiff(v_, tochange, newconf, lt_)));
            if (ratio > distu(rgen_)) {
              accept_[1] += 1;
              psi_.UpdateLookup(v_, tochange, newconf, lt_);
              hilbert_.UpdateConf(v_, tochange, newconf);
            }
          }
        } else {
          // swap rows
          for (int j = 0; j < L_; j++) {
            if ((v_reshape.row(r)(j) - v_reshape.row((r + 1) % L_)(j)) >
                std::numeric_limits<double>::epsilon()) {
              tochange.push_back(j * L_ + r);
              tochange.push_back(j * L_ + (r + 1) % L_);
              newconf.push_back(v_reshape.row((r + 1) % L_)(j));
              newconf.push_back(v_reshape.row(r)(j));
            }
          }
          if (tochange.size() != 0) {
            double ratio = std::norm(
                std::exp(psi_.LogValDiff(v_, tochange, newconf, lt_)));
            if (ratio > distu(rgen_)) {
              accept_[1] += 1;
              psi_.UpdateLookup(v_, tochange, newconf, lt_);
              hilbert_.UpdateConf(v_, tochange, newconf);
            }
          }
        }
        moves_[1] += 1;
      }
    }
  }

  Eigen::VectorXd Visible() override { return v_; }

  void SetVisible(const Eigen::VectorXd &v) override { v_ = v; }

  WfType &Psi() override { return psi_; }

  Eigen::VectorXd Acceptance() const override {
    Eigen::VectorXd acc = accept_;
    for (int i = 0; i < 2; i++) {
      acc(i) /= moves_(i);
    }
    return acc;
  }
};

}  // namespace netket

#endif
