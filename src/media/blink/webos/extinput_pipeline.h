// Copyright (c) 2015-2018 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef MEDIA_BLINK_WEBOS_EXTINPUT_PIPELINE_H_
#define MEDIA_BLINK_WEBOS_EXTINPUT_PIPELINE_H_

#include <string>
#include <cstdint>
#include <memory>

#include <Acb.h>
#include "base/time/time.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "media/base/pipeline.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/blink/webos/custom_pipeline.h"
#include "media/webos/base/lunaservice_client.h"
#include "third_party/WebKit/public/platform/WebRect.h"

class GURL;

namespace media {

class ExtInputPipeline : public CustomPipeline,
                         public base::RefCountedThreadSafe<ExtInputPipeline> {
 public:
  ExtInputPipeline(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  void Load(bool is_video,
            const std::string& app_id,
            const GURL& url,
            const std::string& mime_type,
            const std::string& payload,
            const CustomPipeline::BufferingStateCB& buffering_state_cb,
            const base::Closure& video_display_window_change_cb,
            const CustomPipeline::PipelineMsgCB& pipeline_message_cb) override;
  void Dispose() override;
  void SetPlaybackRate(float playback_rate) override;
  float GetPlaybackRate() const override { return playback_rate_; }
  std::string MediaId() const override { return external_input_id_; }
  double Duration() override { return 0.0; }
  base::TimeDelta CurrentTime() { return base::TimeDelta::FromMilliseconds(0); }
  float BufferEnd() override { return 0.0f; }
  bool HasVideo() override { return has_video_; }
  bool HasAudio() override { return has_audio_; }
  gfx::Size NaturalVideoSize() override { return gfx::Size(1920, 1080); }
  void SetDisplayWindow(const blink::WebRect&,
                        const blink::WebRect&,
                        bool fullScreen,
                        bool forced = false) override;
  bool DidLoadingProgress() override { return false; }
  void ContentsPositionChanged(float position) override;

 private:
  friend class base::RefCountedThreadSafe<ExtInputPipeline>;
  ~ExtInputPipeline();

  bool loaded_;
  bool has_video_;
  bool has_audio_;
  float playback_rate_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  media::LunaServiceClient luna_service_client_;
  std::unique_ptr<Acb> acb_client_;
  long acb_load_task_id_;
  long acb_id_;
  std::string app_id_;
  GURL url_;
  std::string mime_type_;
  base::Closure video_display_window_change_cb_;
  BufferingStateCB buffering_state_cb_;
  PipelineMsgCB pipeline_message_cb_;
  std::string external_input_id_;
  EventCallback event_handler_;
  blink::WebRect out_rect_;
  blink::WebRect source_rect_;
  bool full_screen_;
  LSMessageToken subscribe_key_;
  base::Lock lock_;

  void CallswitchExternalInput();
  void PlayPipeline();
  void PausePipeline();
  void ClosePipeline();
  void GetCurrentVideo();
  void HandleGetCurrentVideoReply(const std::string& payload);
  void AcbLoadCompleted();
  void AcbHandler(long acb_id,
                  long task_id,
                  long event_type,
                  long app_state,
                  long play_state,
                  const char* reply);
  void DispatchAcbHandler(long acb_id,
                          long task_id,
                          long event_type,
                          long app_state,
                          long play_state,
                          const char* reply);

  void HandleOpenReply(const std::string& payload);
  void HandleSwitchExternalInputReply(const std::string& payload);

  DISALLOW_COPY_AND_ASSIGN(ExtInputPipeline);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBOS_EXTINPUT_PIPELINE_H_
