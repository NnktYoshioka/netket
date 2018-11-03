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

#ifndef NETKET_CONVSQUARELAYER_HH
#define NETKET_CONVSQUARELAYER_HH

#include <time.h>
#include <Eigen/Dense>
#include <algorithm>
#include <complex>
#include <fstream>
#include <random>
#include <vector>

#include "Graph/graph.hpp"
#include "Lookup/lookup.hpp"
#include "Utils/all_utils.hpp"
#include "abstract_layer.hpp"

namespace netket {
/** Convolutional layer with spin 1/2 hidden units.
 Important: In order for this to work correctly, VectorType and MatrixType must
 be column major.
 */
template <typename Activation, typename T>
class ConvolutionalSquare : public AbstractLayer<T> {
  using VectorType = typename AbstractLayer<T>::VectorType;

  using MatrixType = typename AbstractLayer<T>::MatrixType;
  static_assert(!MatrixType::IsRowMajor, "MatrixType must be column-major");

  Activation activation_;  // activation function class

  bool usebias_;  // boolean to turn or off bias

  int nv_;
  int nout_;
  int input_image_size_;
  int output_image_size_;
  int stride_;
  int filter_size_;  // square filter length
  int kernel_size_;  // number of parameters in filter = (filter length)^2
  int in_channels_;
  int in_size_;
  int out_channels_;
  int out_size_;
  int npar_;

  std::vector<std::vector<int>>
      neighbours_;  // list of neighbours for each site
  std::vector<std::vector<int>>
      flipped_nodes_;   // list of reverse neighbours for each site
  MatrixType kernels_;  // Weight parameters, W(in_size x out_size)
  VectorType bias_;     // Bias parameters, b(out_size x 1)

  // Note that input of this layer is also the output of
  // previous layer

  MatrixType lowered_image_;
  MatrixType lowered_image2_;
  MatrixType lowered_der_;
  MatrixType flipped_kernels_;

 public:
  using StateType = typename AbstractLayer<T>::StateType;
  using LookupType = typename AbstractLayer<T>::LookupType;

  /// Constructor
  ConvolutionalSquare(const int image_size, const int stride,
                      const int filter_size, const int input_channel,
                      const int output_channel, const bool use_bias = true)
      : activation_(),
        usebias_(use_bias),
        input_image_size_(image_size),
        stride_(stride),
        filter_size_(filter_size),
        in_channels_(input_channel),
        out_channels_(output_channel) {
    Init();
  }

  explicit ConvolutionalSquare(const json &pars) : activation_() {
    usebias_ = FieldOrDefaultVal(pars, "UseBias", true);
    input_image_size_ = FieldVal(pars, "ImageSize");
    stride_ = FieldVal(pars, "Stride");
    filter_size_ = FieldVal(pars, "FilterSize");
    in_channels_ = FieldVal(pars, "InputChannels");
    out_channels_ = FieldVal(pars, "OutputChannels");
    Init();
  }

  void Init() {
    nv_ = input_image_size_ * input_image_size_;
    in_size_ = in_channels_ * nv_;

    // Check that stride_ is compatible with input_image_size_
    if (input_image_size_ % stride_ != 0) {
      throw InvalidInputError(
          "Stride size is incompatiple with input image size: they should be "
          "commensurate");
    }
    output_image_size_ = input_image_size_ / stride_;
    nout_ = output_image_size_ * output_image_size_;
    out_size_ = out_channels_ * nout_;

    kernel_size_ = filter_size_ * filter_size_;

    npar_ = in_channels_ * kernel_size_ * out_channels_;

    if (usebias_) {
      npar_ += out_channels_;
    } else {
      bias_.setZero();
    }

    // Construct neighbourhood of all nodes with distance of at most dist_ from
    // each node i, kernel(k) will act on the node neighbours_[i][k] of the
    // input image to give a value at node i in the output image.
    for (int i = 0; i < nout_; ++i) {
      std::vector<int> neigh;
      int sy = (i / output_image_size_) * stride_;
      int sx = (i % output_image_size_) * stride_;
      for (int j = 0; j < filter_size_; ++j) {
        for (int k = 0; k < filter_size_; ++k) {
          int kx = (sx + k) % input_image_size_;
          int ky = (sy + j) % input_image_size_;
          int index = ky * input_image_size_ + kx;
          neigh.push_back(index);
        }
      }
      neighbours_.push_back(neigh);
    }

    // Construct flipped_nodes_[i][k] = nn such that
    // site i contributes to nn via kernel(k)
    for (int i = 0; i < nv_; ++i) {
      std::vector<int> flippednodes;
      int sy = i / input_image_size_;
      int sx = i % input_image_size_;
      for (int j = 0; j < filter_size_; ++j) {
        for (int k = 0; k < filter_size_; ++k) {
          int kx = ((sx - k) % input_image_size_ + input_image_size_) %
                   input_image_size_;
          int ky = ((sy - j) % input_image_size_ + input_image_size_) %
                   input_image_size_;
          int nx = kx / stride_;
          int ny = ky / stride_;
          // std::cout << "kx = " << kx << " nx = " << nx << std::endl;
          // std::cout << "ky = " << ky << " ny = " << ny << std::endl;
          int index = ny * output_image_size_ + nx;
          if (stride_ * nx == kx && stride_ * ny == ky) {
            flippednodes.push_back(index);
          } else {
            flippednodes.push_back(-1);
          }
        }
      }
      flipped_nodes_.push_back(flippednodes);
    }

    kernels_.resize(in_channels_ * kernel_size_, out_channels_);
    bias_.resize(out_channels_);

    lowered_image_.resize(in_channels_ * kernel_size_, nout_);
    lowered_image2_.resize(nout_, in_channels_ * kernel_size_);
    lowered_der_.resize(kernel_size_ * out_channels_, nv_);
    flipped_kernels_.resize(kernel_size_ * out_channels_, in_channels_);

    std::string buffer = "";

    InfoMessage(buffer) << "Square Convolutional Layer: " << in_size_ << " --> "
                        << out_size_ << std::endl;
    InfoMessage(buffer) << "# # InputChannels = " << in_channels_ << std::endl;
    InfoMessage(buffer) << "# # OutputChannels = " << out_channels_
                        << std::endl;
    InfoMessage(buffer) << "# # Square Filter Length = " << filter_size_
                        << std::endl;
    InfoMessage(buffer) << "# # Stride = " << stride_ << std::endl;
    InfoMessage(buffer) << "# # UseBias = " << usebias_ << std::endl;
  }

  void InitRandomPars(const json &pars) override {
    double sigma = FieldOrDefaultVal(pars, "SigmaRand", 0.1);

    VectorType par(npar_);

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
      for (int i = 0; i < out_channels_; ++i) {
        pars(k) = bias_(i);
        ++k;
      }
    }

    for (int j = 0; j < out_channels_; ++j) {
      for (int i = 0; i < in_channels_ * kernel_size_; ++i) {
        pars(k) = kernels_(i, j);
        ++k;
      }
    }
  }

  void SetParameters(const VectorType &pars, int start_idx) override {
    int k = start_idx;

    if (usebias_) {
      for (int i = 0; i < out_channels_; ++i) {
        bias_(i) = pars(k);
        ++k;
      }
    }

    for (int j = 0; j < out_channels_; ++j) {
      for (int i = 0; i < in_channels_ * kernel_size_; ++i) {
        kernels_(i, j) = pars(k);
        ++k;
      }
    }
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
    // At the moment the light cone structure of the convolution is not
    // exploited. To do so we would to change the part
    // else if (num_of_changes >0) {...}
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

  // performs the convolution of the kernel onto the image and writes into z
  inline void Convolve(const VectorType &image, VectorType &z) {
    // im2col method
    for (int i = 0; i < nout_; ++i) {
      int j = 0;
      for (auto n : neighbours_[i]) {
        for (int in = 0; in < in_channels_; ++in) {
          lowered_image_(in * kernel_size_ + j, i) = image(in * nv_ + n);
        }
        j++;
      }
    }
    Eigen::Map<MatrixType> output_image(z.data(), nout_, out_channels_);
    output_image.noalias() = lowered_image_.transpose() * kernels_;
  }

  // Performs the linear transformation for the layer.
  inline void LinearTransformation(const VectorType &input, LookupType &theta) {
    Convolve(input, theta[0]);

    if (usebias_) {
      int k = 0;
      for (int out = 0; out < out_channels_; ++out) {
        for (int i = 0; i < nout_; ++i) {
          theta[0](k) += bias_(out);
          ++k;
        }
      }
    }
  }

  // Performs the nonlinear transformation for the layer.
  inline void NonLinearTransformation(const LookupType &theta,
                                      VectorType &output) {
    activation_(theta[0], output);
  }

  inline void UpdateTheta(const VectorType &v,
                          const std::vector<int> &input_changes,
                          const VectorType &new_input, LookupType &theta) {
    const int num_of_changes = input_changes.size();

    for (int s = 0; s < num_of_changes; ++s) {
      const int sf = input_changes[s];
      int kout = 0;
      for (int out = 0; out < out_channels_; ++out) {
        for (int k = 0; k < kernel_size_; ++k) {
          if (flipped_nodes_[sf][k] >= 0) {
            theta[0](flipped_nodes_[sf][k] + kout) +=
                kernels_(k, out) * (new_input(s) - v(sf));
          }
        }
        kout += nout_;
      }
    }
  }

  inline void UpdateTheta(const VectorType &prev_input,
                          const std::vector<int> &tochange,
                          const std::vector<double> &newconf,
                          LookupType &theta) {
    const int num_of_changes = tochange.size();
    for (int s = 0; s < num_of_changes; ++s) {
      const int sf = tochange[s];
      int kout = 0;
      for (int out = 0; out < out_channels_; ++out) {
        for (int k = 0; k < kernel_size_; ++k) {
          if (flipped_nodes_[sf][k] >= 0) {
            theta[0](flipped_nodes_[sf][k] + kout) +=
                kernels_(k, out) * (newconf[s] - prev_input(sf));
          }
        }
        kout += nout_;
      }
    }
  }

  void Backprop(const VectorType &prev_layer_output,
                const VectorType &this_layer_output,
                const LookupType &this_layer_theta, const VectorType &dout,
                VectorType &din, VectorType &der, int start_idx) override {
    // Compute dL/dz
    VectorType dLz(out_size_);
    activation_.ApplyJacobian(this_layer_theta[0], this_layer_output, dout,
                              dLz);

    int kd = start_idx;

    // Derivative for bias, d(L) / d(b) = d(L) / d(z)
    if (usebias_) {
      int k = 0;
      for (int out = 0; out < out_channels_; ++out) {
        der(kd) = 0;
        for (int i = 0; i < nout_; ++i) {
          der(kd) += dLz(k);
          ++k;
        }
        ++kd;
      }
    }

    // Derivative for weights, d(L) / d(W) = [d(L) / d(z)] * in'
    // Reshape dLdZ
    Eigen::Map<MatrixType> dLz_reshaped(dLz.data(), nout_, out_channels_);

    // Reshape image
    for (int in = 0; in < in_channels_; ++in) {
      for (int k = 0; k < kernel_size_; ++k) {
        for (int i = 0; i < nout_; ++i) {
          lowered_image2_(i, k + in * kernel_size_) =
              prev_layer_output(in * nv_ + neighbours_[i][k]);
        }
      }
    }
    Eigen::Map<MatrixType> der_w(der.data() + kd, in_channels_ * kernel_size_,
                                 out_channels_);
    der_w.noalias() = lowered_image2_.transpose() * dLz_reshaped;

    // Compute d(L) / d_in = W * [d(L) / d(z)]
    int kout = 0;
    for (int out = 0; out < out_channels_; ++out) {
      for (int in = 0; in < in_channels_; ++in) {
        for (int k = 0; k < kernel_size_; ++k) {
          flipped_kernels_(k + kout, in) = kernels_(k + in * kernel_size_, out);
        }
      }
      kout += kernel_size_;
    }
    for (int i = 0; i < nv_; i++) {
      int j = 0;
      for (auto n : flipped_nodes_[i]) {
        for (int out = 0; out < out_channels_; ++out) {
          lowered_der_(out * kernel_size_ + j, i) =
              n >= 0 ? dLz(out * nout_ + n) : 0;
        }
        j++;
      }
    }
    din.resize(in_size_);
    Eigen::Map<MatrixType> der_in(din.data(), nv_, in_channels_);
    der_in.noalias() = lowered_der_.transpose() * flipped_kernels_;
  }

  void to_json(json &pars) const override {
    json layerpar;
    layerpar["Name"] = "Convolutional";
    layerpar["UseBias"] = usebias_;
    layerpar["Inputs"] = in_size_;
    layerpar["Outputs"] = out_size_;
    layerpar["InputChannels"] = in_channels_;
    layerpar["OutputChannels"] = out_channels_;
    layerpar["Bias"] = bias_;
    layerpar["Kernels"] = kernels_;

    pars["Machine"]["Layers"].push_back(layerpar);
  }

  void from_json(const json &pars) override {
    if (FieldExists(pars, "Kernels")) {
      kernels_ = pars["Kernels"];
    } else {
      kernels_.setZero();
    }
    if (FieldExists(pars, "Bias")) {
      bias_ = pars["Bias"];
    } else {
      bias_.setZero();
    }
  }
};
}  // namespace netket

#endif
