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

#ifndef NETKET_VARIATIONALEXACT_HPP
#define NETKET_VARIATIONALEXACT_HPP

#include <Eigen/Dense>
#include <Eigen/IterativeLinearSolvers>
#include <algorithm>
#include <complex>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "Machine/lanczos.hpp"
#include "Machine/machine.hpp"
#include "Observable/observable.hpp"
#include "Optimizer/optimizer.hpp"
#include "Sampler/sampler.hpp"
#include "Stats/stats.hpp"
#include "Utils/parallel_utils.hpp"
#include "Utils/random_utils.hpp"
#include "json_output_writer.hpp"
#include "matrix_replacement.hpp"

namespace netket {

// Variational Monte Carlo schemes to learn the ground state
// Available methods:
// 1) Stochastic reconfiguration optimizer
//   both direct and sparse version
// 2) Gradient Descent optimizer
template <class WfType>
class VariationalExact {
  using GsType = std::complex<double>;

  using VectorT = Eigen::Matrix<typename WfType::StateType, Eigen::Dynamic, 1>;
  using MatrixT =
      Eigen::Matrix<typename WfType::StateType, Eigen::Dynamic, Eigen::Dynamic>;

  Hamiltonian &ham_;
  Sampler<WfType> &sampler_;
  WfType &psi_;

  // Essential for exact evaluations
  const Hilbert &hilbert_;
  const HilbertIndex hilbert_index_;
  int dim_;
  int nv_;

  std::vector<std::vector<int>> connectors_;
  std::vector<std::vector<double>> newconfs_;
  std::vector<std::complex<double>> mel_;

  Eigen::VectorXcd elocs_;
  MatrixT Ok_;
  VectorT Okmean_;
  MatrixT Ok1_;
  MatrixT Ok2_;
  VectorT psi1_;
  VectorT psi2_;

  Eigen::MatrixXd vsamp_;

  Eigen::VectorXcd grad_;
  Eigen::VectorXcd gradprev_;

  double sr_diag_shift_;
  bool sr_rescale_shift_;
  bool use_iterative_;

  int totalnodes_;
  int mynode_;

  // This optional will contain a value iff the MPI rank is 0.
  nonstd::optional<JsonOutputWriter> output_;

  Optimizer &opt_;

  std::vector<Observable> obs_;
  ObsManager obsmanager_;

  bool dosr_;

  bool use_cholesky_;

  int nsamples_;
  int nsamples_node_;
  int ninitsamples_;
  int ndiscardedsamples_;
  int niter_opt_;

  std::complex<double> elocmean_;
  double elocvar_;
  int npar_;

 public:
  // JSON constructor
  VariationalExact(Hamiltonian &ham, Sampler<WfType> &sampler, Optimizer &opt,
                   const json &pars)
      : ham_(ham),
        sampler_(sampler),
        psi_(sampler.Psi()),
        hilbert_(ham.GetHilbert()),
        hilbert_index_(hilbert_),
        nv_(psi_.Nvisible()),
        opt_(opt),
        obs_(Observable::FromJson(ham.GetHilbert(), pars)),
        elocvar_(0.) {
    // compute dim_
    dim_ = 1;
    for (int i = 0; i < nv_ / 2; ++i) {
      dim_ *= (nv_ - i);
    }
    for (int i = 0; i < nv_ / 2; ++i) {
      dim_ /= (i + 1);
    }

    // DEPRECATED (to remove for v2.0.0)
    if (FieldExists(pars, "Learning")) {
      auto pars1 = pars;
      pars1["GroundState"] = pars["Learning"];
      Init(pars1);
    } else {
      Init(pars);
    }
    InitOutput(pars);
  }

  void InitOutput(const json &pars) {
    // DEPRECATED (to remove for v2.0.0)
    auto pars_gs = FieldExists(pars, "GroundState") ? pars["GroundState"]
                                                    : pars["Learning"];
    if (mynode_ == 0) {
      output_ = JsonOutputWriter::FromJson(pars_gs);
    }
  }

  void Init(const json &pars) {
    npar_ = psi_.Npar();

    opt_.Init(psi_.GetParameters());

    grad_.resize(npar_);
    Okmean_.resize(npar_);

    psi2_.resize(dim_);
    psi2_.setZero();
    psi1_.resize(dim_);
    psi1_.setZero();

    setSrParameters();

    MPI_Comm_size(MPI_COMM_WORLD, &totalnodes_);
    MPI_Comm_rank(MPI_COMM_WORLD, &mynode_);

    nsamples_ = FieldVal(pars["GroundState"], "Nsamples", "GroundState");

    nsamples_node_ = int(std::ceil(double(nsamples_) / double(totalnodes_)));

    ninitsamples_ =
        FieldOrDefaultVal(pars["GroundState"], "DiscardedSamplesOnInit", 0.);

    ndiscardedsamples_ = FieldOrDefaultVal(
        pars["GroundState"], "DiscardedSamples", 0.1 * nsamples_node_);

    niter_opt_ = FieldVal(pars["GroundState"], "NiterOpt", "GroundState");

    if (pars["GroundState"]["Method"] == "GdExact") {
      dosr_ = false;
    } else {
      double diagshift =
          FieldOrDefaultVal(pars["GroundState"], "DiagShift", 0.01);
      bool rescale_shift =
          FieldOrDefaultVal(pars["GroundState"], "RescaleShift", false);
      bool use_iterative =
          FieldOrDefaultVal(pars["GroundState"], "UseIterative", false);
      use_cholesky_ =
          FieldOrDefaultVal(pars["GroundState"], "UseCholesky", true);
      setSrParameters(diagshift, rescale_shift, use_iterative);
    }

    if (dosr_) {
      InfoMessage() << "Using the Stochastic reconfiguration method"
                    << std::endl;

      if (use_iterative_) {
        InfoMessage() << "With iterative solver" << std::endl;
      } else {
        if (use_cholesky_) {
          InfoMessage() << "Using Cholesky decomposition" << std::endl;
        }
      }
    } else {
      InfoMessage() << "Using a gradient-descent based method" << std::endl;
    }

    InfoMessage() << "Exact Variational running on " << totalnodes_
                  << " processes" << std::endl;

    MPI_Barrier(MPI_COMM_WORLD);
  }

  void InitSweeps() {
    sampler_.Reset();
    for (int i = 0; i < ninitsamples_; i++) {
      sampler_.Sweep();
    }
  }

  void GetConfig() {
    vsamp_.resize(dim_, psi_.Nvisible());

    std::vector<int> myints;
    for (int i = 0; i < nv_ / 2; ++i) {
      myints.push_back(1);
      myints.push_back(-1);
    }

    std::sort(myints.begin(), myints.end());

    int count = 0;
    do {
      Eigen::VectorXd v(nv_);
      v.setZero();

      for (int j = 0; j < nv_; j++) {
        v(j) = myints[j];
      }

      vsamp_.row(count) = v;
      // cout << "config " << count << " = " << v.transpose() << endl;
      count++;
    } while (std::next_permutation(myints.begin(), myints.end()));
    // InfoMessage() << "Full set of configurations obtained" << std::endl;
    // InfoMessage() << "Hilbert space dimensions = " << dim_ << std::endl;
  }

  void Sample() {
    // vsamp_.resize(dim_, psi_.Nvisible());
    // for (int i = 0; i < dim_; ++i) {
    //   vsamp_.row(i) = hilbert_index_.NumberToState(i);
    // }
    GetConfig();
    InfoMessage() << "Full set of configurations obtained" << std::endl;
    InfoMessage() << "Hilbert space dimensions = " << dim_ << std::endl;
  }

  void Gradient() {
    obsmanager_.Reset("Energy");
    obsmanager_.Reset("EnergyVariance");

    for (const auto &ob : obs_) {
      obsmanager_.Reset(ob.Name());
    }

    const int nsamp = vsamp_.rows();
    elocs_.resize(nsamp);
    Ok_.resize(nsamp, psi_.Npar());

    elocmean_ = 0.0;
    double norm = 0.0;
    for (int i = 0; i < dim_; i++) {
      elocs_(i) = Eloc(vsamp_.row(i));
      Ok_.row(i) = psi_.DerLog(vsamp_.row(i));
      norm += std::norm(std::exp(psi_.LogVal(vsamp_.row(i))));
      std::complex<double> lv = psi_.LogVal(vsamp_.row(i));
      psi2_(i) = std::norm(std::exp(lv));
      psi1_(i) = std::exp(lv);
    }
    psi2_ /= norm;
    psi1_ /= std::sqrt(norm);
    elocmean_ = (elocs_.transpose() * psi2_)(0);
    // for (int i = 0; i < nsamp; i++) {
    //   obsmanager_.Push("Energy", elocmean_.real());
    // }
    obsmanager_.Push("Energy", elocmean_.real());

    for (int i = 0; i < npar_; ++i) {
      std::complex<double> mean = 0.0;
      for (int j = 0; j < dim_; ++j) {
        mean += Ok_.col(i)(j) * psi2_(j);
      }
      Okmean_(i) = mean;
    }

    Ok_ = Ok_.rowwise() - Okmean_.transpose();

    elocs_ -= elocmean_ * Eigen::VectorXd::Ones(nsamp);

    // for (int i = 0; i < nsamp; i++) {
    //   double x =
    //       std::abs(((psi2_.asDiagonal() * elocs_).adjoint() * elocs_)(0));
    //   obsmanager_.Push("EnergyVariance", x);
    // }
    double x = std::abs(((psi2_.asDiagonal() * elocs_).adjoint() * elocs_)(0));
    std::cout << "elocmean = " << elocmean_ << std::endl;
    std::cout << "x = " << x << std::endl;
    obsmanager_.Push("EnergyVariance", x);

    grad_ = 2. * (Ok_.adjoint() * elocs_);

    // Summing the gradient over the nodes
    SumOnNodes(grad_);
    grad_ /= double(totalnodes_ * nsamp);
  }

  std::complex<double> Eloc(const Eigen::VectorXd &v) {
    ham_.FindConn(v, mel_, connectors_, newconfs_);

    assert(connectors_.size() == mel_.size());

    auto logvaldiffs = (psi_.LogValDiff(v, connectors_, newconfs_));

    assert(mel_.size() == std::size_t(logvaldiffs.size()));

    std::complex<double> eloc = 0;

    for (int i = 0; i < logvaldiffs.size(); i++) {
      eloc += mel_[i] * std::exp(logvaldiffs(i));
    }

    return eloc;
  }

  double ObSamp(const Observable &ob, const Eigen::VectorXd &v) {
    ob.FindConn(v, mel_, connectors_, newconfs_);

    assert(connectors_.size() == mel_.size());

    auto logvaldiffs = (psi_.LogValDiff(v, connectors_, newconfs_));

    assert(mel_.size() == std::size_t(logvaldiffs.size()));

    std::complex<double> obval = 0;

    for (int i = 0; i < logvaldiffs.size(); i++) {
      obval += mel_[i] * std::exp(logvaldiffs(i));
    }

    return obval.real();
  }

  double ElocMean() { return elocmean_.real(); }

  double Elocvar() { return elocvar_; }

  void Run() {
    Sample();
    for (int i = 0; i < niter_opt_; i++) {
      Gradient();

      UpdateParameters();

      PrintOutput(i);
    }
  }

  void UpdateParameters() {
    auto pars = psi_.GetParameters();

    if (dosr_) {
      const int nsamp = vsamp_.rows();

      Eigen::VectorXcd b = Ok_.adjoint() * psi2_.asDiagonal() * elocs_;
      // SumOnNodes(b);
      // b /= double(nsamp * totalnodes_);

      if (!use_iterative_) {
        // Explicit construction of the S matrix
        Eigen::MatrixXcd S = Ok_.adjoint() * psi2_.asDiagonal() * Ok_;
        // SumOnNodes(S);
        // S /= double(nsamp * totalnodes_);

        // Adding diagonal shift
        S += Eigen::MatrixXd::Identity(pars.size(), pars.size()) *
             sr_diag_shift_;

        Eigen::VectorXcd deltaP;
        if (use_cholesky_ == false) {
          Eigen::FullPivHouseholderQR<Eigen::MatrixXcd> qr(S.rows(), S.cols());
          qr.setThreshold(1.0e-6);
          qr.compute(S);
          deltaP = qr.solve(b);
        } else {
          Eigen::LLT<Eigen::MatrixXcd> llt(S.rows());
          llt.compute(S);
          deltaP = llt.solve(b);
        }
        // Eigen::VectorXcd deltaP=S.jacobiSvd(ComputeThinU |
        // ComputeThinV).solve(b);

        assert(deltaP.size() == grad_.size());
        grad_ = deltaP;

        if (sr_rescale_shift_) {
          std::complex<double> nor = (deltaP.dot(S * deltaP));
          grad_ /= std::sqrt(nor.real());
        }

      } else {
        Eigen::ConjugateGradient<MatrixReplacement, Eigen::Lower | Eigen::Upper,
                                 Eigen::IdentityPreconditioner>
            it_solver;
        // Eigen::GMRES<MatrixReplacement, Eigen::IdentityPreconditioner>
        // it_solver;
        it_solver.setTolerance(1.0e-3);
        MatrixReplacement S;
        S.attachMatrix(Ok_);
        S.setShift(sr_diag_shift_);
        S.setScale(1. / double(nsamp * totalnodes_));

        it_solver.compute(S);
        auto deltaP = it_solver.solve(b);

        grad_ = deltaP;
        if (sr_rescale_shift_) {
          auto nor = deltaP.dot(S * deltaP);
          grad_ /= std::sqrt(nor.real());
        }

        // if(mynode_==0){
        //   std::cerr<<it_solver.iterations()<<"
        //   "<<it_solver.error()<<std::endl;
        // }
        MPI_Barrier(MPI_COMM_WORLD);
      }
    }

    opt_.Update(grad_, pars);

    SendToAll(pars);

    psi_.SetParameters(pars);
    MPI_Barrier(MPI_COMM_WORLD);
  }

  void PrintOutput(int i) {
    // Note: This has to be called in all MPI processes, because converting
    // the ObsManager to JSON performs a MPI reduction.
    auto obs_data = json(obsmanager_);
    obs_data["Acceptance"] = sampler_.Acceptance();
    obs_data["GradNorm"] = grad_.norm();
    obs_data["MaxPar"] = psi_.GetParameters().array().abs().maxCoeff();

    if (output_.has_value()) {  // output_.has_value() iff the MPI rank is 0, so
                                // the output is only written once
      output_->WriteLog(i, obs_data);
      output_->WriteState(i, psi_);
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }

  void setSrParameters(double diagshift = 0.01, bool rescale_shift = false,
                       bool use_iterative = false) {
    sr_diag_shift_ = diagshift;
    sr_rescale_shift_ = rescale_shift;
    use_iterative_ = use_iterative;
    dosr_ = true;
  }

  void CheckDerLog(double eps = 1.0e-4) {
    std::cout << "# Debugging Derivatives of Wave-Function Logarithm"
              << std::endl;
    std::flush(std::cout);

    sampler_.Reset(true);
    auto ders = psi_.DerLog(sampler_.Visible());
    auto pars = psi_.GetParameters();
    for (int i = 0; i < npar_; i++) {
      pars(i) += eps;
      psi_.SetParameters(pars);
      std::complex<double> valp = psi_.LogVal(sampler_.Visible());

      pars(i) -= 2 * eps;
      psi_.SetParameters(pars);
      std::complex<double> valm = psi_.LogVal(sampler_.Visible());

      pars(i) += eps;

      std::complex<double> numder = (-valm + valp) / (eps * 2);

      if (std::abs(numder - ders(i)) > eps * eps) {
        std::cerr << " Possible error on parameter " << i
                  << ". Expected: " << ders(i) << " Found: " << numder
                  << std::endl;
      }
    }
    std::cout << "# Test completed" << std::endl;
    std::flush(std::cout);
  }
};

}  // namespace netket

#endif