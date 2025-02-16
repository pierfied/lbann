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

#ifndef LBANN_TRANSFORMS_RANDOM_RESIZED_CROP_WITH_FIXED_ASPECT_RATIO_HPP_INCLUDED
#define LBANN_TRANSFORMS_RANDOM_RESIZED_CROP_WITH_FIXED_ASPECT_RATIO_HPP_INCLUDED

#include "lbann/transforms/transform.hpp"

#include <google/protobuf/message.h>

namespace lbann {
namespace transform {

/** Resize an image then extract a random crop. */
class random_resized_crop_with_fixed_aspect_ratio : public transform {
public:
  /** Resize to h x w, then extract a random crop_h x crop_w crop. */
  random_resized_crop_with_fixed_aspect_ratio(
    size_t h, size_t w, size_t crop_h, size_t crop_w) :
    transform(), m_h(h), m_w(w), m_crop_h(crop_h), m_crop_w(crop_w) {}

  transform* copy() const override {
    return new random_resized_crop_with_fixed_aspect_ratio(*this);
  }

  std::string get_type() const override {
    return "random_resized_crop_with_fixed_aspect_ratio";
  }

  void apply(utils::type_erased_matrix& data, std::vector<size_t>& dims) override;
private:
  /** Height and width of the resized image. */
  size_t m_h, m_w;
  /** Height and width of the crop. */
  size_t m_crop_h, m_crop_w;
};

std::unique_ptr<transform>
build_random_resized_crop_with_fixed_aspect_ratio_transform_from_pbuf(
  google::protobuf::Message const&);

}  // namespace transform
}  // namespace lbann

#endif  // LBANN_TRANSFORMS_RANDOM_RESIZED_CROP_WITH_FIXED_ASPECT_RATIO_HPP_INCLUDED
