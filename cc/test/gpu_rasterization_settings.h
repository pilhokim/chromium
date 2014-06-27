// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_GPU_RASTERIZATION_SETTINGS_H_
#define CC_TEST_GPU_RASTERIZATION_SETTINGS_H_

#include "cc/trees/layer_tree_settings.h"

namespace cc {

class GpuRasterizationSettings : public LayerTreeSettings {
 public:
  GpuRasterizationSettings() {
    impl_side_painting = true;
    rasterization_site = GpuRasterization;
  }
};

}  // namespace cc

#endif  // CC_TEST_GPU_RASTERIZATION_SETTINGS_H_
