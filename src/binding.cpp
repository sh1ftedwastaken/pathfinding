#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "VoxelGrid.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pathfinding, m)
{
    py::class_<VoxelGrid>(m, "VoxelGrid")
          .def(py::init([](py::buffer grid, const std::vector<coord>& metadata)
               {
                    py::buffer_info voxel_grid = grid.request(true);

                    if (voxel_grid.ptr == nullptr || voxel_grid.size == 0)
                         throw py::value_error("Voxel grid is empty");

                    return VoxelGrid(static_cast<int*>(voxel_grid.ptr), metadata);
               }),
               py::keep_alive<1, 2>(),
               py::arg("voxel_grid"), py::arg("metadata"))

          .def("getIndex", &VoxelGrid::getIndex,
               py::arg("x"), py::arg("y"), py::arg("z"))
          .def("extractPreProcessVoxel", &VoxelGrid::extractPreProcessVoxel,
               "Mark hazards and return the best ore cluster center, or None.")
          .def("DijkstraCosts", &VoxelGrid::DijkstraCosts,
               py::arg("start_pos"), py::arg("targets"))
          .def("AStart", &VoxelGrid::AStart,
               py::arg("start_pos"), py::arg("end_pos"))
          .def("Dijkstra", &VoxelGrid::Dijkstra,
               py::arg("start_pos"), py::arg("end_pos"));
}