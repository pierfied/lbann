////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014-2022, Lawrence Livermore National Security, LLC.
// Produced at the Lawrence Livermore National Laboratory.
// Written by the LBANN Research Team (B. Van Essen, et al.) listed in
// the CONTRIBUTORS file. <lbann-dev@llnl.gov>
//
// LLNL-CODE-697807.
// All rights reserved.
//
// This file is part of LBANN: Livermore Big Artificial Neural Network
// Toolkit. For details, see http://software.llnl.gov/LBANN or
// https://github.com/LLNL/LBANN.
//
// Licensed under the Apache License, Version 2.0 (the "Licensee"); you
// may not use this file except in compliance with the License.  You may
// obtain a copy of the License at:
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the license.
////////////////////////////////////////////////////////////////////////////////

#define LBANN_CROSS_ENTROPY_LAYER_INSTANTIATE
#include "lbann/layers/loss/cross_entropy.hpp"
#include "lbann/utils/exception.hpp"

namespace lbann {

namespace {

template <typename TensorDataType>
void local_fp_cpu(const El::AbstractMatrix<TensorDataType>& local_prediction,
                  const El::AbstractMatrix<TensorDataType>& local_ground_truth,
                  El::AbstractMatrix<TensorDataType>& local_contribution) {

  // Useful constants
  const TensorDataType zero = El::TypeTraits<TensorDataType>::Zero();
  const El::Int local_height = local_prediction.Height();
  const El::Int local_width = local_prediction.Width();

  // Compute local contribution to cross entropy
  LBANN_OMP_PARALLEL_FOR
  for (El::Int col = 0; col < local_width; ++col) {
    TensorDataType sum = zero;
    for (El::Int row = 0; row < local_height; ++row) {
      const auto& xhat = local_ground_truth(row, col);
      if (xhat > zero) {
        const auto& x = local_prediction(row, col);
#ifdef LBANN_DEBUG
        if (x <= zero) { LBANN_ERROR("non-positive prediction"); }
#endif // LBANN_DEBUG
        sum += - xhat * std::log(x);
      }
    }
    local_contribution(0, col) = sum;
  }

}

template <typename TensorDataType>
void local_bp_cpu(const El::AbstractMatrix<TensorDataType>& local_prediction,
                  const El::AbstractMatrix<TensorDataType>& local_ground_truth,
                  const El::AbstractMatrix<TensorDataType>& local_gradient_wrt_output,
                  El::AbstractMatrix<TensorDataType>& local_gradient_wrt_prediction,
                  El::AbstractMatrix<TensorDataType>& local_gradient_wrt_ground_truth) {

  // Useful constants
  const TensorDataType zero = El::TypeTraits<TensorDataType>::Zero();
  const El::Int local_height = local_prediction.Height();
  const El::Int local_width = local_prediction.Width();

  // Compute gradients
  LBANN_OMP_PARALLEL_FOR_COLLAPSE2
  for (El::Int col = 0; col < local_width; ++col) {
    for (El::Int row = 0; row < local_height; ++row) {
      const auto& x = local_prediction(row, col);
      const auto& xhat = local_ground_truth(row, col);
      const auto& dy = local_gradient_wrt_output(0, col);
      auto& dx = local_gradient_wrt_prediction(row, col);
      auto& dxhat = local_gradient_wrt_ground_truth(row, col);
      dx = (xhat > zero) ? - dy * xhat / x : zero;
      dxhat = - dy * std::log(x);
    }
  }

}

} // namespace

template <typename TensorDataType, data_layout T_layout, El::Device Dev>
void cross_entropy_layer<TensorDataType, T_layout, Dev>::local_fp_compute() {
  local_fp_cpu(this->get_local_prev_activations(0),
               this->get_local_prev_activations(1),
               this->m_workspace->Matrix());
}

template <typename TensorDataType, data_layout T_layout, El::Device Dev>
void cross_entropy_layer<TensorDataType, T_layout, Dev>::local_bp_compute() {
  local_bp_cpu(this->get_local_prev_activations(0),
               this->get_local_prev_activations(1),
               this->m_workspace->LockedMatrix(),
               this->get_local_error_signals(0),
               this->get_local_error_signals(1));
}

#define PROTO(T)                                      \
  template class cross_entropy_layer<                 \
    T, data_layout::DATA_PARALLEL, El::Device::CPU>;  \
  template class cross_entropy_layer<                 \
    T, data_layout::MODEL_PARALLEL, El::Device::CPU>

#define LBANN_INSTANTIATE_CPU_HALF
#include "lbann/macros/instantiate.hpp"

} // namespace lbann
