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

#include "Layer/abstract_layer.hpp"
#include "Layer/activations.hpp"
#include "Layer/conv_layer.hpp"
#include "Layer/conv_square_layer.hpp"
#include "Layer/fullconn_layer.hpp"
#include "Layer/sum_output.hpp"

#ifndef NETKET_LAYER_HPP
#define NETKET_LAYER_HPP

namespace netket {
template <class T>
class Layer : public AbstractLayer<T> {
  using Ptype = std::unique_ptr<AbstractLayer<T>>;

  Ptype m_;

 public:
  using VectorType = typename AbstractMachine<T>::VectorType;
  using MatrixType = typename AbstractMachine<T>::MatrixType;
  using StateType = typename AbstractMachine<T>::StateType;
  using LookupType = std::vector<VectorType>;

  explicit Layer(const Graph &graph, const json &pars) { Init(graph, pars); }

  void Init(const Graph &graph, const json &pars) {
    CheckInput(pars);
    if (pars["Name"] == "FullyConnected") {
      if (pars["Activation"] == "Lncosh") {
        m_ = Ptype(new FullyConnected<Lncosh, T>(pars));
      } else if (pars["Activation"] == "Identity") {
        m_ = Ptype(new FullyConnected<Identity, T>(pars));
      } else if (pars["Activation"] == "Tanh") {
        m_ = Ptype(new FullyConnected<Tanh, T>(pars));
      } else if (pars["Activation"] == "Relu") {
        m_ = Ptype(new FullyConnected<Relu, T>(pars));
      }
    } else if (pars["Name"] == "Convolutional") {
      if (pars["Activation"] == "Lncosh") {
        m_ = Ptype(new Convolutional<Lncosh, T>(graph, pars));
      } else if (pars["Activation"] == "Identity") {
        m_ = Ptype(new Convolutional<Identity, T>(graph, pars));
      } else if (pars["Activation"] == "Tanh") {
        m_ = Ptype(new Convolutional<Tanh, T>(graph, pars));
      } else if (pars["Activation"] == "Relu") {
        m_ = Ptype(new Convolutional<Relu, T>(graph, pars));
      }
    } else if (pars["Name"] == "SquareConvolutional") {
      if (pars["Activation"] == "Lncosh") {
        m_ = Ptype(new ConvolutionalSquare<Lncosh, T>(pars));
      } else if (pars["Activation"] == "Identity") {
        m_ = Ptype(new ConvolutionalSquare<Identity, T>(pars));
      } else if (pars["Activation"] == "Tanh") {
        m_ = Ptype(new ConvolutionalSquare<Tanh, T>(pars));
      } else if (pars["Activation"] == "Relu") {
        m_ = Ptype(new ConvolutionalSquare<Relu, T>(pars));
      }
    } else if (pars["Name"] == "Sum") {
      m_ = Ptype(new SumOutput<T>(pars));
    }
  }

  void CheckInput(const json &pars) {
    int mynode;
    MPI_Comm_rank(MPI_COMM_WORLD, &mynode);

    const std::string name = FieldVal(pars, "Name");

    std::set<std::string> layers = {"FullyConnected", "Convolutional",
                                    "Symmetric", "Sum", "SquareConvolutional"};

    if (layers.count(name) == 0) {
      std::stringstream s;
      s << "Unknown Machine: " << name;
      throw InvalidInputError(s.str());
    }
  }

  int Npar() const override { return m_->Npar(); }

  int Ninput() const override { return m_->Ninput(); }

  int Noutput() const override { return m_->Noutput(); }

  void GetParameters(VectorType &pars, int start_idx) const override {
    return m_->GetParameters(pars, start_idx);
  }

  void SetParameters(const VectorType &pars, int start_idx) override {
    return m_->SetParameters(pars, start_idx);
  }

  void InitRandomPars(const json &pars) override {
    return m_->InitRandomPars(pars);
  }

  void InitLookup(const VectorType &v, LookupType &lt,
                  VectorType &output) override {
    return m_->InitLookup(v, lt, output);
  }

  void UpdateLookup(const VectorType &input,
                    const std::vector<int> &input_changes,
                    const VectorType &new_input, LookupType &theta,
                    const VectorType &output, std::vector<int> &output_changes,
                    VectorType &new_output) override {
    return m_->UpdateLookup(input, input_changes, new_input, theta, output,
                            output_changes, new_output);
  }

  void UpdateLookup(const Eigen::VectorXd &input,
                    const std::vector<int> &tochange,
                    const std::vector<double> &newconf, LookupType &theta,
                    const VectorType &output, std::vector<int> &output_changes,
                    VectorType &new_output) override {
    return m_->UpdateLookup(input, tochange, newconf, theta, output,
                            output_changes, new_output);
  }

  void Forward(const VectorType &prev_layer_output, LookupType &theta,
               VectorType &output) override {
    return m_->Forward(prev_layer_output, theta, output);
  }

  void Forward(const LookupType &theta, VectorType &output) override {
    return m_->Forward(theta, output);
  }

  void Backprop(const VectorType &prev_layer_output,
                const VectorType &this_layer_output,
                const LookupType &this_layer_theta, const VectorType &dout,
                VectorType &din, VectorType &der, int start_idx) override {
    return m_->Backprop(prev_layer_output, this_layer_output, this_layer_theta,
                        dout, din, der, start_idx);
  }

  void to_json(json &j) const override { m_->to_json(j); }

  void from_json(const json &j) override { m_->from_json(j); }
};
}  // namespace netket
#endif
