#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include <unordered_map>
#include <queue>
#include <vector>
#include <stdexcept>
#include <tuple>
#include <cstdint>

namespace py = pybind11;

enum Block {
    FIFO,
    COST,
    NUM_BLOCKS
};

class AdaptivePipeLineCache {
private:
    size_t getsizeof(const std::tuple<double, uint64_t>& value) {
        return 1;
    }

public:
    explicit AdaptivePipeLineCache(size_t maxsize){
        throw std::runtime_error("Not implemented");
    }

    std::tuple<double, uint64_t> getitem(uint64_t key) {
        throw std::runtime_error("Not implemented");
    }

    void setitem(uint64_t key, const std::tuple<double, uint64_t>& value) {
        throw std::runtime_error("Not implemented");
    }

    void delitem(uint64_t key) {
        throw std::runtime_error("Not implemented");
    }

    bool contains(uint64_t key) const {
        throw std::runtime_error("Not implemented");
    }

    std::pair<uint64_t, std::tuple<double, uint64_t>> popitem() {
        throw std::runtime_error("Not implemented");
    }

    std::tuple<double, uint64_t> get(uint64_t key, const std::tuple<double, uint64_t>& default_value = std::make_tuple(0.0, 0)) {
        throw std::runtime_error("Not implemented");
    }

    std::vector<uint64_t> keys() const {
        throw std::runtime_error("Not implemented");
    }

    std::vector<std::tuple<double, uint64_t>> values() const {
        throw std::runtime_error("Not implemented");
    }

    size_t maxsize() const { throw std::runtime_error("Not implemented"); }
    size_t currsize() const { throw std::runtime_error("Not implemented"); }
    size_t size() const { throw std::runtime_error("Not implemented"); }
    bool empty() const { throw std::runtime_error("Not implemented"); }

    void clear() {
        throw std::runtime_error("Not implemented");
    }

    std::string repr() const {
        throw std::runtime_error("Not implemented");
    }
};

// IMPORTANT: The module name here MUST match the name in setup.py ext_modules
PYBIND11_MODULE(_adaptive_pipeline_cache_impl, m) {
    m.doc() = "Internal C++ implementation of The Pipeline Cache";

    py::class_<AdaptivePipeLineCache>(m, "AdaptivePipeLineCacheImpl")
        .def(py::init<size_t>(), "Initialize Pipeline cache with maximum size")
        .def("__getitem__", &AdaptivePipeLineCache::getitem)
        .def("__setitem__", &AdaptivePipeLineCache::setitem)
        .def("__delitem__", &AdaptivePipeLineCache::delitem)
        .def("__contains__", &AdaptivePipeLineCache::contains)
        .def("__len__", &AdaptivePipeLineCache::size)
        .def("__repr__", &AdaptivePipeLineCache::repr)
        .def("popitem", &AdaptivePipeLineCache::popitem)
        .def("get", &AdaptivePipeLineCache::get, py::arg("key"), py::arg("default") = std::make_tuple(0.0, 0))
        .def("keys", &AdaptivePipeLineCache::keys)
        .def("values", &AdaptivePipeLineCache::values)
        .def("clear", &AdaptivePipeLineCache::clear)
        .def_property_readonly("maxsize", &AdaptivePipeLineCache::maxsize)
        .def_property_readonly("currsize", &AdaptivePipeLineCache::currsize)
        .def("empty", &AdaptivePipeLineCache::empty);
}
