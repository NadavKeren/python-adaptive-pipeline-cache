#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>

#include "adaptive_pipeline_cache.cpp"

namespace py = pybind11;

void init_adaptive_pipeline_cache(py::module &m) {
    py::class_<AdaptivePipelineCache>(m, "AdaptivePipelineCacheImpl")
        .def(py::init<size_t>(), "Initialize Pipeline cache with maximum size")
        .def("__getitem__", &AdaptivePipelineCache::getitem)
        .def("__setitem__", &AdaptivePipelineCache::setitem)
        .def("__delitem__", &AdaptivePipelineCache::delitem)
        .def("__contains__", &AdaptivePipelineCache::contains)
        .def("__len__", &AdaptivePipelineCache::currsize)
        .def("__repr__", &AdaptivePipelineCache::repr)
        .def("popitem", &AdaptivePipelineCache::popitem)
        .def("get", &AdaptivePipelineCache::get, py::arg("key"), py::arg("default") = std::make_tuple(0.0, 0))
        .def("keys", &AdaptivePipelineCache::keys)
        .def("values", &AdaptivePipelineCache::values)
        .def("clear", &AdaptivePipelineCache::clear)
        .def_property_readonly("maxsize", &AdaptivePipelineCache::maxsize)
        .def_property_readonly("currsize", &AdaptivePipelineCache::currsize)
        .def("empty", &AdaptivePipelineCache::empty);
}

PYBIND11_MODULE(_adaptive_pipeline_cache_impl, m) {
    m.doc() = "Internal C++ implementation of The Adaptive Pipeline Cache";
    
    init_adaptive_pipeline_cache(m);
}
