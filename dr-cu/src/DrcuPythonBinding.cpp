/**
 * @file   DrcuPythonBinding.cpp
 * @author Yibo Lin
 * @date   Feb 2020
 */

#include <pybind11/stl.h>
//#include <pybind11/stl_bind.h>
//#include <pybind11/numpy.h>
#include "Drcu.h"

//PYBIND11_MAKE_OPAQUE(std::vector<std::string>);
//PYBIND11_MAKE_OPAQUE(std::vector<double>);
//PYBIND11_MAKE_OPAQUE(std::vector<float>);
//PYBIND11_MAKE_OPAQUE(std::vector<int>);
//PYBIND11_MAKE_OPAQUE(std::vector<std::vector<double>>);
//PYBIND11_MAKE_OPAQUE(std::array<double, 4>);

PYBIND11_MODULE(drcu_cpp, m) {
    //pybind11::bind_vector<std::vector<std::string> >(m, "VectorString");
    //pybind11::bind_vector<std::vector<double> >(m, "VectorDouble");
    //pybind11::bind_vector<std::vector<float> >(m, "VectorFloat");
    //pybind11::bind_vector<std::vector<int> >(m, "VectorInt");
    //pybind11::bind_vector<std::vector<std::vector<double> > >(m, "VectorDoubleNested");
    //pybind11::bind_vector<std::array<double, 4> >(m, "ArrayDoubleX4");

    pybind11::class_<Drcu::Res> (m, "DrcuRes")
        .def(pybind11::init<>())
        .def_readwrite("feature", &Drcu::Res::feature)
        .def_readwrite("done", &Drcu::Res::done)
        .def_readwrite("reward", &Drcu::Res::reward)
        ;

    pybind11::class_<Drcu> (m, "Drcu")
        .def(pybind11::init<>())
        .def("init", (void (Drcu::*)(std::vector<std::string> const& argv)) &Drcu::init)
        .def("reset", &Drcu::reset)
        .def("test", (void (Drcu::*)(std::vector<std::string> const& argv)) &Drcu::test)
        .def("step", (Drcu::Res (Drcu::*)(std::vector<double> const& action)) &Drcu::step)
        .def("get_the_1st_observation", (std::vector<std::vector<double>> (Drcu::*)()) &Drcu::get_the_1st_observation)
        .def("get_all_vio", (std::array<double, 4> (Drcu::*)() const) &Drcu::get_all_vio)
        ;
}
