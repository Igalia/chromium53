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

#ifndef MEDIA_BLINK_WEBOS_WEBMEDIAPLAYER_CUSTOM_H_
#define MEDIA_BLINK_WEBOS_WEBMEDIAPLAYER_CUSTOM_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "cc/layers/video_frame_provider.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_keys.h"
#include "media/base/pipeline.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webos/camera_pipeline.h"
#include "media/blink/webos/dvr_pipeline.h"
#include "media/blink/webos/extinput_pipeline.h"
#include "media/blink/webos/photo_pipeline.h"
#include "media/blink/webos/tv_pipeline.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebAudioSourceProvider.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "url/gurl.h"

namespace blink {
class WebLocalFrame;
}

namespace base {
class MessageLoopProxy;
}

namespace cc_blink {
class WebLayerImpl;
}

namespace media {
class MediaLog;
class WebAudioSourceProviderImpl;
class WebMediaPlayerDelegate;
class WebMediaPlayerParams;

class WebMediaPlayerCustom : public blink::WebMediaPlayer,
                             public cc::VideoFrameProvider,
                             public base::MessageLoop::DestructionObserver,
                             public base::SupportsWeakPtr<WebMediaPlayerCustom>,
                             public media::WebMediaPlayerDelegate::Observer {
 public:
  // Constructs a WebMediaPlayer implementation
  // using uMediaServer WebOS media stack.
  //
  // |delegate| may be null.
  WebMediaPlayerCustom(blink::WebLocalFrame* frame,
                       blink::WebMediaPlayerClient* client,
                       base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
                       const media::WebMediaPlayerParams& params,
                       blink::WebFloatPoint additionalContentsScale,
                       const blink::WebString& app_id);
  virtual ~WebMediaPlayerCustom();

  virtual void load(LoadType load_type,
                    const blink::WebMediaPlayerSource& src,
                    CORSMode cors_mode) override;

  // Playback controls.
  virtual void play();
  virtual void pause();
  virtual bool supportsFullscreen() const;
  virtual bool supportsSave() const;
  virtual void seek(double seconds);
  virtual void setRate(double rate);
  virtual void setVolume(double volume);
  virtual blink::WebTimeRanges buffered() const;
  virtual blink::WebTimeRanges seekable() const;
  virtual double maxTimeSeekable() const;

  // WebMediaPlayer interface stubs
  virtual void setSinkId(const blink::WebString& sinkId,
                         const blink::WebSecurityOrigin&,
                         blink::WebSetSinkIdCallbacks*) override {}
  virtual blink::WebString getErrorMessage() override {
    return blink::WebString();
  }

  // Methods for painting.
  virtual void paint(blink::WebCanvas* canvas,
                     const blink::WebRect& rect,
                     unsigned char alpha,
                     SkXfermode::Mode mode);

  // True if the loaded media has a playable video/audio track.
  virtual bool hasVideo() const;
  virtual bool hasAudio() const;

  // Dimensions of the video.
  virtual blink::WebSize naturalSize() const;

  // Getters of playback state.
  virtual bool paused() const;
  virtual bool seeking() const;
  virtual double duration() const;
  virtual double currentTime() const;

  // Internal states of loading and network.
  virtual blink::WebMediaPlayer::NetworkState getNetworkState() const override;
  virtual blink::WebMediaPlayer::ReadyState getReadyState() const override;

  virtual bool didLoadingProgress();

  virtual bool hasSingleSecurityOrigin() const;
  virtual bool didPassCORSAccessCheck() const;

  virtual double mediaTimeForTimeValue(double timeValue) const;

  virtual unsigned decodedFrameCount() const;
  virtual unsigned droppedFrameCount() const;
  virtual unsigned audioDecodedByteCount() const;
  virtual unsigned videoDecodedByteCount() const;

  // cc::VideoFrameProvider implementation.
  virtual void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) override;
  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() override;
  virtual void PutCurrentFrame() override;
  bool UpdateCurrentFrame(base::TimeTicks deadline_min,
                          base::TimeTicks deadline_max) override;
  bool HasCurrentFrame() override;

  virtual bool copyVideoTextureToPlatformTexture(
      gpu::gles2::GLES2Interface* web_graphics_context,
      unsigned int texture,
      unsigned int internal_format,
      unsigned int type,
      bool premultiply_alpha,
      bool flip_y);

  virtual blink::WebAudioSourceProvider* getAudioSourceProvider() override;

  // As we are closing the tab or even the browser, |main_loop_| is destroyed
  // even before this object gets destructed, so we need to know when
  // |main_loop_| is being destroyed and we can stop posting repaint task
  // to it.
  virtual void WillDestroyCurrentMessageLoop() override;

  virtual blink::WebString cameraId() const override;
  virtual blink::WebString mediaId() const override;

  // WebMediaPlayerDelegate::Observer interface stubs
  virtual void OnHidden() override {}
  virtual void OnShown() override {}
  virtual void OnSuspendRequested(bool must_suspend) override {}
  virtual void OnPlay() override {}
  virtual void OnPause() override {}
  virtual void OnVolumeMultiplierUpdate(double multiplier) override {}

  void Repaint();

  void OnPipelineBufferingState(CustomPipeline::BufferingState buffering_state);

  void OnPipelineMessage(const std::string& detail);
  void SetOpaque(bool opaque);

  virtual bool usesIntrinsicSize() override;
  void paintTimerFired(void);
  virtual void updateVideo(const blink::WebRect&, bool) override;
  virtual void suspend() override;
  virtual void resume() override;
  virtual void contentsPositionChanged(float position) override;

 private:
  // Called after |defer_load_cb_| has decided to allow the load. If
  // |defer_load_cb_| is null this is called immediately.
  void DoLoad(LoadType load_type, const blink::WebURL& url, CORSMode cors_mode);

  // Called after asynchronous initialization of a data source completed.
  void DataSourceInitialized(const GURL& gurl, bool success);

  // Called when the data source is downloading or paused.
  void NotifyDownloading(bool is_downloading);

  // Finishes starting the pipeline due to a call to load().
  void StartPipeline();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(blink::WebMediaPlayer::NetworkState state);
  void SetReadyState(blink::WebMediaPlayer::ReadyState state);

  // Destroy resources held.
  void Destroy();

  // Getter method to |client_|.
  blink::WebMediaPlayerClient* GetClient();

  // Gets the duration value reported by the pipeline.
  double GetPipelineDuration() const;

  void OnVideoDisplayWindowChange();

  blink::WebLocalFrame* frame_;

  // TODO(hclam): get rid of these members and read from the pipeline directly.
  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  // Keep a list of buffered time ranges.
  blink::WebTimeRanges buffered_;

  // Message loops for posting tasks on Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  base::MessageLoop* main_loop_;

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // Playback state.
  bool paused_;
  bool seeking_;
  double playback_rate_;
  base::TimeDelta paused_time_;

  // Seek gets pending if another seek is in progress. Only last pending seek
  // will have effect.
  bool pending_seek_;
  double pending_seek_seconds_;

  blink::WebMediaPlayerClient* client_;

  base::WeakPtr<media::WebMediaPlayerDelegate> delegate_;
  int delegate_id_ = 0;

  base::Callback<void(const base::Closure&)> defer_load_cb_;

  scoped_refptr<media::MediaLog> media_log_;

  scoped_refptr<media::WebAudioSourceProviderImpl> audio_source_provider_;

  bool is_local_source_;
  bool supports_save_;

  bool starting_;

  bool is_suspended_;

  // Video frame rendering members.
  //
  // |lock_| protects |current_frame_| since new frames arrive on the video
  // rendering thread, yet are accessed for rendering on either the main thread
  // or compositing thread depending on whether accelerated compositing is used.
  base::Lock lock_;
  scoped_refptr<media::VideoFrame> current_frame_;
  bool pending_size_change_;

  // The compositor layer for displaying the video content when using composited
  // playback.
  std::unique_ptr<cc_blink::WebLayerImpl> video_weblayer_;

  // A pointer back to the compositor to inform it about state changes. This is
  // not NULL while the compositor is actively using this webmediaplayer.
  cc::VideoFrameProvider::Client* video_frame_provider_client_;

  base::RepeatingTimer paintTimer_;
  media::CustomPipeline* custom_pipeline_;

  bool fullscreen_;
  blink::WebRect display_window_out_rect_;
  blink::WebRect display_window_in_rect_;
  blink::WebRect previous_video_rect_;
  const blink::WebFloatPoint additional_contents_scale_;
  blink::WebRect ScaleWebRect(const blink::WebRect& rect,
                              blink::WebFloatPoint scale);
  std::string app_id_;
  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerCustom);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBOS_WEBMEDIAPLAYER_CUSTOM_H_
