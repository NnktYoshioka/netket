#include "Stats/stats.hpp"

#include <iomanip>

#include <pybind11/eigen.h>
#include <pybind11/pybind11.h>

#include "Stats/mc_stats.hpp"
#include "Stats/obs_manager.hpp"
#include "Utils/exceptions.hpp"
#include "common_types.hpp"

namespace py = pybind11;

namespace netket {
namespace detail {

py::dict GetItem(const ObsManager& self, const std::string& name) {
  py::dict dict;
  self.InsertAllStats(name, dict);
  return dict;
}

int GetPrecision(double /* value */, double error) {
  const int loge = std::floor(std::log10(std::abs(error)));
  return std::max(1 - loge, 0);
}

int GetPrecision(Complex value, double error) {
  return GetPrecision(std::max(std::abs(value.real()), std::abs(value.imag())),
                      error);
}

void AddStatsModule(py::module m) {
  auto subm = m.def_submodule("stats");

  py::class_<ObsManager>(subm, "ObsManager")
      .def("__getitem__", &detail::GetItem, py::arg("name"))
      .def("__getattr__", &detail::GetItem, py::arg("name"))
      .def("__contains__", &ObsManager::Contains, py::arg("name"))
      .def("__len__", &ObsManager::Size)
      .def("keys", &ObsManager::Names)
      .def("__repr__", [](const ObsManager& self) {
        std::string s("<netket.stats.ObsManager: size=");
        auto size = self.Size();
        s += std::to_string(size);
        if (size > 0) {
          s += " [";
          for (const auto& name : self.Names()) {
            s += name + ", ";
          }
          // remove last comma + space:
          s.pop_back();
          s.pop_back();

          s += "]";
        }
        return s + ">";
      });

  auto as_dict = [](const Stats& self) {
    py::dict d;
    d["Mean"] = self.mean;
    d["Sigma"] = self.error_of_mean;
    d["Variance"] = self.variance;
    d["R"] = self.R;
    d["TauCorr"] = self.correlation;
    return d;
  };

  py::class_<Stats>(subm, "Stats")
      .def_readonly("mean", &Stats::mean)
      .def_readonly("error_of_mean", &Stats::error_of_mean)
      .def_readonly("variance", &Stats::variance)
      .def_readonly("tau_corr", &Stats::correlation)
      .def_readonly("R", &Stats::R)
      .def("__repr__",
           [](const Stats& self) {
             std::ostringstream stream;
             const double imag = self.mean.imag();
             const int precision = GetPrecision(self.mean, self.error_of_mean);
             // clang-format off
             stream << std::fixed << std::setprecision(precision)
                    << "(" << self.mean.real()
                    << (imag >= 0 ? " + " : " - ") << std::abs(imag)
                    << "i) ± " << self.error_of_mean
                    << " [var=" << self.variance
                    << ", R=" << std::setprecision(5) << self.R
                    << "]";
             // clang-format on
             return stream.str();
           })
      .def("_asdict", as_dict)  //< compatibility with namedtuple
      .def("asdict", as_dict);

  subm.def(
      "statistics",
      [](py::array_t<Complex, py::array::c_style> local_values) {
        switch (local_values.ndim()) {
          case 2:
            return Statistics(
                Eigen::Map<const Eigen::VectorXcd>{local_values.data(),
                                                   local_values.size()},
                /*n_chains=*/local_values.shape(1));
          case 1:
            return Statistics(
                Eigen::Map<const Eigen::VectorXcd>{local_values.data(),
                                                   local_values.size()},
                /*n_chains=*/1);
          default:
            NETKET_CHECK(false, InvalidInputError,
                         "local_values has wrong dimension: "
                             << local_values.ndim()
                             << "; expected either 1 or 2.");
        }  // end switch
      },
      py::arg{"values"}.noconvert(),
      R"EOF(Computes some statistics (see `Stats` class) of a sequence of
            local estimators obtained from Monte Carlo sampling.

            Args:
                values: A tensor of local estimators. It can be either a rank-1
                    or a rank-2 tensor of `complex128`. Rank-1 tensors represent
                    data from a single Markov Chain, so e.g. `error_on_mean` will
                    be `None`.

                    Rank-2 tensors should have shape `(N, M)` where `N` is the
                    number of samples in one Markov Chain and `M` is the number
                    of Markov Chains. Data should be in row major order.)EOF");
}
}  // namespace detail
}  // namespace netket

namespace netket {

void AddStatsModule(PyObject* m) {
  detail::AddStatsModule(py::module{py::reinterpret_borrow<py::object>(m)});
}

}  // namespace netket
