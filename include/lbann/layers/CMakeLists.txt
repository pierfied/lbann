################################################################################
## Copyright (c) 2014-2022, Lawrence Livermore National Security, LLC.
## Produced at the Lawrence Livermore National Laboratory.
## Written by the LBANN Research Team (B. Van Essen, et al.) listed in
## the CONTRIBUTORS file. <lbann-dev@llnl.gov>
##
## LLNL-CODE-697807.
## All rights reserved.
##
## This file is part of LBANN: Livermore Big Artificial Neural Network
## Toolkit. For details, see http://software.llnl.gov/LBANN or
## https://github.com/LLNL/LBANN.
##
## Licensed under the Apache License, Version 2.0 (the "Licensee"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at:
##
## http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
## implied. See the License for the specific language governing
## permissions and limitations under the license.
################################################################################
# Add the headers for this directory
set_full_path(THIS_DIR_HEADERS
  data_type_layer.hpp
  layer.hpp
  operator_layer.hpp
  )

if (LBANN_HAS_DISTCONV)
  list(APPEND THIS_DIR_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/distconv_adapter.hpp")
  list(APPEND THIS_DIR_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/data_type_distconv_adapter.hpp")
endif ()

# Add the subdirectories
add_subdirectory(activations)
add_subdirectory(image)
add_subdirectory(io)
add_subdirectory(learning)
add_subdirectory(loss)
add_subdirectory(math)
add_subdirectory(misc)
add_subdirectory(regularizers)
add_subdirectory(transform)

# Propagate the files up the tree
set(HEADERS "${HEADERS}" "${THIS_DIR_HEADERS}" PARENT_SCOPE)
