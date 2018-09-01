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

#ifndef NETKET_CUSTOM_GRAPH_HPP
#define NETKET_CUSTOM_GRAPH_HPP

#include <mpi.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <unordered_map>
#include <vector>
#include "Hilbert/hilbert.hpp"
#include "Utils/all_utils.hpp"

namespace netket {

/**
    Class for user-defined graphs
    The list of edges and nodes is read from a json input file.
*/
class CustomGraph : public AbstractGraph {
  // adjacency list
  std::vector<std::vector<int>> adjlist_;

  ColorMap eclist_;

  int nsites_;

  std::vector<std::vector<int>> automorphisms_;

  bool isbipartite_;
  bool is_connected_;

 public:
  // Json constructor
  explicit CustomGraph(const json &pars) { Init(pars); }

  void Init(const json &pars) {
    // Try to construct from explicit graph definition
    if (FieldExists(pars, "Graph")) {
      if (FieldExists(pars["Graph"], "Edges")) {
        std::vector<std::vector<int>> edges =
            pars["Graph"]["Edges"].get<std::vector<std::vector<int>>>();
        AdjacencyListFromEdges(edges);
      }
      if (FieldExists(pars["Graph"], "Size")) {
        assert(pars["Graph"]["Size"] > 0);
        adjlist_.resize(pars["Graph"]["Size"]);
      }
      if (FieldExists(pars["Graph"], "AdjacencyList")) {
        adjlist_ =
            pars["Graph"]["AdjacencyList"].get<std::vector<std::vector<int>>>();
      }
    } else if (FieldExists(pars, "Hilbert")) {
      Hilbert hilbert(pars);
      nsites_ = hilbert.Size();
      assert(nsites_ > 0);
      adjlist_.resize(nsites_);
    } else {
      throw InvalidInputError(
          "Graph: one among Size, AdjacencyList, Edges, or Hilbert "
          "Space Size must be specified");
    }

    nsites_ = adjlist_.size();

    automorphisms_.resize(1, std::vector<int>(nsites_));
    for (int i = 0; i < nsites_; i++) {
      // If no automorphism is specified, we stick to the identity one
      automorphisms_[0][i] = i;
    }

    isbipartite_ = false;
    is_connected_ = ComputeConnected();

    // Other graph properties
    if (FieldExists(pars, "Graph")) {
      if (FieldExists(pars["Graph"], "Automorphisms")) {
        automorphisms_ =
            pars["Graph"]["Automorphisms"].get<std::vector<std::vector<int>>>();
      }

      if (FieldExists(pars["Graph"], "IsBipartite")) {
        isbipartite_ = pars["Graph"]["IsBipartite"];
      }

      // If edge colors are specificied read them in, otherwise set them all to
      // 0
      if (FieldExists(pars["Graph"], "EdgeColors")) {
        std::vector<std::vector<int>> colorlist =
            pars["Graph"]["EdgeColors"].get<std::vector<std::vector<int>>>();
        EdgeColorsFromList(colorlist, eclist_);
      } else {
        InfoMessage() << "No colors specified, edge colors set to 0 "
                      << std::endl;
        EdgeColorsFromAdj(adjlist_, eclist_);
      }
    }

    CheckGraph();

    InfoMessage() << "Graph created " << std::endl;
    InfoMessage() << "Number of nodes = " << nsites_ << std::endl;
  }

  void AdjacencyListFromEdges(const std::vector<std::vector<int>> &edges) {
    nsites_ = 0;

    for (auto edge : edges) {
      if (edge.size() != 2) {
        throw InvalidInputError(
            "The edge list is invalid (edges need "
            "to connect exactly two sites)");
      }
      if (edge[0] < 0 || edge[1] < 0) {
        throw InvalidInputError("The edge list is invalid");
      }

      nsites_ = std::max(std::max(edge[0], edge[1]), nsites_);
    }

    nsites_++;
    adjlist_.resize(nsites_);

    for (auto edge : edges) {
      adjlist_[edge[0]].push_back(edge[1]);
      adjlist_[edge[1]].push_back(edge[0]);
    }
  }

  void CheckGraph() {
    for (int i = 0; i < nsites_; i++) {
      for (auto s : adjlist_[i]) {
        // Checking if the referenced nodes are within the expected range
        if (s >= nsites_ || s < 0) {
          throw InvalidInputError("The graph is invalid");
        }
        // Checking if the adjacency list is symmetric
        // i.e. if site s is declared neihgbor of site i
        // when site i is declared neighbor of site s
        if (std::count(adjlist_[s].begin(), adjlist_[s].end(), i) != 1) {
          throw InvalidInputError("The graph adjacencylist is not symmetric");
        }
      }
    }
    for (std::size_t i = 0; i < automorphisms_.size(); i++) {
      if (int(automorphisms_[i].size()) != nsites_) {
        throw InvalidInputError("The automorphism list is invalid");
      }
    }
  }

  // Returns a list of permuted sites constituting an automorphism of the
  // graph
  std::vector<std::vector<int>> SymmetryTable() const override {
    return automorphisms_;
  }

  int Nsites() const override { return nsites_; }

  std::vector<std::vector<int>> AdjacencyList() const override {
    return adjlist_;
  }

  bool IsBipartite() const override { return isbipartite_; }

  bool IsConnected() const override { return is_connected_; }

  // Returns map of the edge and its respective color
  const ColorMap &EdgeColors() const override { return eclist_; }

 private:
  bool ComputeConnected() const {
    const int start = 0;  // arbitrary node
    int nvisited = 0;
    BreadthFirstSearch(start, [&nvisited](int, int) { ++nvisited; });
    return nvisited == Nsites();
  }
};

}  // namespace netket
#endif
