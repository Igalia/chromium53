// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_preferences.h"

#include "base/sys_info.h"

namespace gpu {

GpuPreferences::GpuPreferences() {
  gpu_program_cache_size = kDefaultMaxProgramCacheMemoryBytes;
#if defined(OS_ANDROID) || defined(OS_WEBOS)
  if (base::SysInfo::IsLowEndDevice())
    gpu_program_cache_size = kLowEndMaxProgramCacheMemoryBytes;
#endif
}

GpuPreferences::GpuPreferences(const GpuPreferences& other) = default;

GpuPreferences::~GpuPreferences() {}

}  // namespace gpu
