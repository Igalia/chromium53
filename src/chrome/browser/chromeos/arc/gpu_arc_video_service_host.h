// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_GPU_ARC_VIDEO_SERVICE_HOST_H_
#define CHROME_BROWSER_CHROMEOS_ARC_GPU_ARC_VIDEO_SERVICE_HOST_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/video.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

// This class takes requests for creating channels of video accelerators from
// arc::VideoInstance and forwards these requests to GpuArcVideoServce. It also
// returns the created channels back to the arc::VideoInstance.
//
// This class is the proxy end of GpuArcVideoService and runs in the browser
// process. The corresponding end "GpuArcVideoService" runs in the GPU process.
class GpuArcVideoServiceHost : public arc::ArcService,
                               public arc::ArcBridgeService::Observer,
                               public arc::mojom::VideoHost {
 public:
  explicit GpuArcVideoServiceHost(arc::ArcBridgeService* bridge_service);
  ~GpuArcVideoServiceHost() override;

  // arc::ArcBridgeService::Observer implementation.
  void OnVideoInstanceReady() override;

  // arc::mojom::VideoHost implementation.
  void DeprecatedOnRequestArcVideoAcceleratorChannel(
      const DeprecatedOnRequestArcVideoAcceleratorChannelCallback& callback)
      override;
  void OnBootstrapVideoAcceleratorFactory(
      const OnBootstrapVideoAcceleratorFactoryCallback& callback) override;

 private:
  base::ThreadChecker thread_checker_;

  mojo::Binding<arc::mojom::VideoHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(GpuArcVideoServiceHost);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_GPU_ARC_VIDEO_SERVICE_HOST_H_
