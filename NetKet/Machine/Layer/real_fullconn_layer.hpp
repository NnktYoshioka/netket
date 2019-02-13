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

#ifndef NETKET_REALFULLCONNLAYER_HH
#define NETKET_REALFULLCONNLAYER_HH

#include <Eigen/Dense>
#include <complex>
#include <fstream>
#include <random>
#include <vector>
#include "Lookup/lookup.hpp"
#include "Utils/all_utils.hpp"
#include "abstract_layer.hpp"

namespace netket {

template <typename Activation, typename T>
class RealFullyConnected : public AbstractLayer<T> {
  using VectorType = typename AbstractLayer<T>::VectorType;
  using MatrixType = typename AbstractLayer<T>::MatrixType;

  Activation activation_;  // activation function class

  bool usebias_;

  int in_size_;                 // input size
  int out_size_;                // output size
  int npar_;                    // number of parameters in layer
  Eigen::MatrixXd realweight_;  // Weight parameters, W(in_size x out_size)
  Eigen::MatrixXd imagweight_;  // Weight parameters, W(in_size x out_size)

  Eigen::MatrixXd weight_;  // Weight parameters, W(in_size x out_size)
  Eigen::VectorXd bias_;

 public:
  using StateType = typename AbstractLayer<T>::StateType;
  using LookupType = typename AbstractLayer<T>::LookupType;

  /// Constructor
  RealFullyConnected(const int input_size, const int output_size,
                     const bool use_bias = false)
      : activation_(),
        usebias_(use_bias),
        in_size_(input_size),
        out_size_(output_size) {
    Init();
  }

  explicit RealFullyConnected(const json &pars) : activation_() { Init(pars); }

  void Init(const json &pars) {
    in_size_ = FieldVal(pars, "Inputs");
    out_size_ = FieldVal(pars, "Outputs");

    usebias_ = FieldOrDefaultVal(pars, "UseBias", true);

    weight_.resize(in_size_, out_size_);
    bias_.resize(out_size_);

    realweight_.resize(in_size_ / 2, out_size_ / 2);
    imagweight_.resize(in_size_ / 2, out_size_ / 2);

    npar_ = in_size_ * out_size_ / 2;

    if (usebias_) {
      npar_ += out_size_;
    } else {
      bias_.setZero();
    }
    std::string buffer = "";

    InfoMessage(buffer) << "Real Fully Connected Layer " << in_size_ << " --> "
                        << out_size_ << std::endl;
    InfoMessage(buffer) << "# # UseBias = " << usebias_ << std::endl;
  }

  void Init() {
    weight_.resize(in_size_, out_size_);
    bias_.resize(out_size_);

    realweight_.resize(in_size_ / 2, out_size_ / 2);
    imagweight_.resize(in_size_ / 2, out_size_ / 2);

    npar_ = in_size_ * out_size_ / 2;

    if (usebias_) {
      npar_ += out_size_;
    } else {
      bias_.setZero();
    }

    std::string buffer = "";

    InfoMessage(buffer) << "Real Fully Connected Layer " << in_size_ << " --> "
                        << out_size_ << std::endl;
    InfoMessage(buffer) << "# # UseBias = " << usebias_ << std::endl;
  }

  void to_json(json &pars) const override {
    json layerpar;
    layerpar["Name"] = "FullyConnected";
    layerpar["UseBias"] = usebias_;
    layerpar["Inputs"] = in_size_;
    layerpar["Outputs"] = out_size_;
    layerpar["Bias"] = bias_;
    layerpar["Weight"] = weight_;

    pars["Machine"]["Layers"].push_back(layerpar);
  }

  void from_json(const json &pars) override {
    if (FieldExists(pars, "Weight")) {
      weight_ = pars["Weight"];
    } else {
      weight_.setZero();
    }
    if (FieldExists(pars, "Bias")) {
      bias_ = pars["Bias"];
    } else {
      bias_.setZero();
    }
  }

  void InitRandomPars(const json &pars) override {
    double sigma = FieldOrDefaultVal(pars, "SigmaRand", 0.1);

    Eigen::VectorXcd par(npar_);

    netket::RandomGaussian(par, 1232, sigma);

    SetParameters(par, 0);

    InfoMessage() << "parameters initialized with random Gaussian: Sigma = "
                  << sigma << std::endl;
  }

  int Npar() const override { return npar_; }

  int Ninput() const override { return in_size_; }

  int Noutput() const override { return out_size_; }

  void GetParameters(VectorType &pars, int start_idx) const override {
    int k = start_idx;

    if (usebias_) {
      pars.segment(k, out_size_) = bias_;
      k += out_size_;
    }

    pars.segment(k, in_size_ * out_size_ / 4) =
        Eigen::Map<const Eigen::VectorXd>(realweight_.data(),
                                          in_size_ * out_size_ / 4);

    k += in_size_ * out_size_ / 4;
    pars.segment(k, in_size_ * out_size_ / 4) =
        Eigen::Map<const Eigen::VectorXd>(imagweight_.data(),
                                          in_size_ * out_size_ / 4);
  }

  void SetParameters(const VectorType &pars, int start_idx) override {
    int k = start_idx;

    if (usebias_) {
      bias_ = pars.segment(k, out_size_).real();
      k += out_size_;
    }

    Eigen::Map<Eigen::VectorXd> rw{realweight_.data(),
                                   in_size_ * out_size_ / 4};
    rw = pars.segment(k, in_size_ * out_size_ / 4).real();
    k += in_size_ * out_size_ / 4;

    Eigen::Map<Eigen::VectorXd> iw{imagweight_.data(),
                                   in_size_ * out_size_ / 4};
    iw = pars.segment(k, in_size_ * out_size_ / 4).real();

    weight_.block(0, 0, in_size_ / 2, out_size_ / 2) = realweight_;
    weight_.block(0, out_size_ / 2, in_size_ / 2, out_size_ / 2) = -imagweight_;
    weight_.block(in_size_ / 2, 0, in_size_ / 2, out_size_ / 2) = imagweight_;
    weight_.block(in_size_ / 2, out_size_ / 2, in_size_ / 2, out_size_ / 2) =
        realweight_;
  }

  void InitLookup(const VectorType &v, LookupType &lt,
                  VectorType &output) override {
    lt.resize(1);
    lt[0].resize(out_size_);

    Forward(v, lt, output);
  }

  void UpdateLookup(const VectorType &input,
                    const std::vector<int> &input_changes,
                    const VectorType &new_input, LookupType &theta,
                    const VectorType & /*output*/,
                    std::vector<int> &output_changes,
                    VectorType &new_output) override {
    const int num_of_changes = input_changes.size();
    if (num_of_changes == in_size_) {
      output_changes.resize(out_size_);
      new_output.resize(out_size_);
      Forward(new_input, theta, new_output);
    } else if (num_of_changes > 0) {
      UpdateTheta(input, input_changes, new_input, theta);
      output_changes.resize(out_size_);
      new_output.resize(out_size_);
      Forward(theta, new_output);
    } else {
      output_changes.resize(0);
      new_output.resize(0);
    }
  }

  void UpdateLookup(const Eigen::VectorXd &input,
                    const std::vector<int> &tochange,
                    const std::vector<double> &newconf, LookupType &theta,
                    const VectorType & /*output*/,
                    std::vector<int> &output_changes,
                    VectorType &new_output) override {
    const int num_of_changes = tochange.size();
    if (num_of_changes > 0) {
      UpdateTheta(input, tochange, newconf, theta);
      output_changes.resize(out_size_);
      new_output.resize(out_size_);
      Forward(theta, new_output);
    } else {
      output_changes.resize(0);
      new_output.resize(0);
    }
  }

  // Feedforward
  void Forward(const VectorType &prev_layer_output, LookupType &theta,
               VectorType &output) override {
    LinearTransformation(prev_layer_output, theta);
    NonLinearTransformation(theta, output);
  }

  // Feedforward Using lookup
  void Forward(const LookupType &theta, VectorType &output) override {
    // Apply activation function
    NonLinearTransformation(theta, output);
  }

  // Applies the linear transformation
  inline void LinearTransformation(const VectorType &input, LookupType &theta) {
    theta[0] = bias_;
    theta[0].noalias() += weight_.transpose() * input;
  }

  // Applies the nonlinear transformation
  inline void NonLinearTransformation(const LookupType &theta,
                                      VectorType &output) {
    activation_(theta[0], output);
  }

  // Updates theta given the input v, the change in the input (input_changes and
  // prev_input)
  inline void UpdateTheta(const VectorType &v,
                          const std::vector<int> &input_changes,
                          const VectorType &new_input, LookupType &theta) {
    const int num_of_changes = input_changes.size();
    for (int s = 0; s < num_of_changes; s++) {
      const int sf = input_changes[s];
      theta[0] += weight_.row(sf) * (new_input(s) - v(sf));
    }
  }

  // Updates theta given the previous input prev_input and the change in the
  // input (tochange and  newconf)
  inline void UpdateTheta(const VectorType &prev_input,
                          const std::vector<int> &tochange,
                          const std::vector<double> &newconf,
                          LookupType &theta) {
    const int num_of_changes = tochange.size();
    for (int s = 0; s < num_of_changes; s++) {
      const int sf = tochange[s];
      theta[0] += weight_.row(sf) * (newconf[s] - prev_input(sf));
    }
  }

  // Computes derivative.
  void Backprop(const VectorType &prev_layer_output,
                const VectorType &this_layer_output,
                const LookupType &this_layer_theta, const VectorType &dout,
                VectorType &din, VectorType &der, int start_idx) override {
    // After forward stage, m_z contains z = W' * in + b
    // Now we need to calculate d(L) / d(z) = [d(a) / d(z)] * [d(L) / d(a)]
    // d(L) / d(a) is computed in the next layer, contained in next_layer_data
    // The Jacobian matrix J = d(a) / d(z) is determined by the activation
    // function
    VectorType dLz(out_size_);
    activation_.ApplyJacobian(this_layer_theta[0], this_layer_output, dout,
                              dLz);

    // Now dLz contains d(L) / d(z)
    // Derivative for bias, d(L) / d(b) = d(L) / d(z)
    int k = start_idx;

    if (usebias_) {
      Eigen::Map<VectorType> der_b{der.data() + k, out_size_};

      der_b.noalias() = dLz;
      k += out_size_;
    }

    // Derivative for weights, d(L) / d(W) = [d(L) / d(z)] * in'
    Eigen::Map<MatrixType>(der.data() + k, in_size_ / 2, out_size_ / 2) =
        prev_layer_output.segment(0, in_size_ / 2) *
            dLz.transpose().segment(0, out_size_ / 2) +
        prev_layer_output.segment(in_size_ / 2, in_size_ / 2) *
            dLz.transpose().segment(out_size_ / 2, out_size_ / 2);
    k += in_size_ * out_size_ / 4;
    Eigen::Map<MatrixType>(der.data() + k, in_size_ / 2, out_size_ / 2) =
        -prev_layer_output.segment(0, in_size_ / 2) *
            dLz.transpose().segment(out_size_ / 2, out_size_ / 2) +
        prev_layer_output.segment(in_size_ / 2, in_size_ / 2) *
            dLz.transpose().segment(0, out_size_ / 2);

    // Compute d(L) / d_in = W * [d(L) / d(z)]
    din.noalias() = weight_ * dLz;
  }
};
}  // namespace netket

#endif