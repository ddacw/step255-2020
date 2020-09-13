// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THUMBNAILER_SRC_THUMBNAILER_H_
#define THUMBNAILER_SRC_THUMBNAILER_H_

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

#include "../imageio/image_dec.h"
#include "../imageio/imageio_util.h"
#include "../imageio/webpdec.h"
#include "webp/encode.h"
#include "webp/mux.h"

#ifdef THUMBNAILER_USE_CMAKE
#include "thumbnailer.pb.h"
#else
#include "src/thumbnailer.pb.h"
#endif

#define CHECK_THUMBNAILER_STATUS(status)    \
  do {                                      \
    const Thumbnailer::Status S = (status); \
    if (S != Thumbnailer::kOk) return S;    \
  } while (0);

namespace libwebp {

// Takes time stamped images as an input and produces an animation.
class Thumbnailer {
 public:
  Thumbnailer();
  Thumbnailer(const thumbnailer::ThumbnailerOption& thumbnailer_option);
  ~Thumbnailer();

  // Status codes for adding frame and generating animation.
  enum [[nodiscard]] Status{
      kOk = 0,            // On success.
      kMemoryError,       // In case of memory error.
      kImageFormatError,  // If frame dimensions are mismatched.
      kByteBudgetError,   // If there is no quality that makes the animation fit
                          // the byte budget.
      kStatsError,    // In case of error while getting frame's size and PSNR.
      kWebPMuxError,  // In case of error related to WebPMux object.
      kSlopeOptimError,  // In case of error while using slope optimization to
                         // generate animation.
      kGenericError      // For other errors.
  };

  enum Method {
    kEqualQuality = 0,
    kEqualPSNR,
    kNearllEqual,
    kNearllDiff,
    kSlopeOptim
  };
  static constexpr Method kMethodList[] = {
      kEqualQuality, kEqualPSNR, kNearllEqual, kNearllDiff, kSlopeOptim};

  // Adds a frame with a timestamp (in millisecond). The 'pic' argument must
  // outlive the last GenerateAnimation() call.
  Status AddFrame(const WebPPicture& pic, int timestamp_ms);

  // Generates the animation using the specified method.
  Status GenerateAnimation(WebPData* const webp_data,
                           Method method = kEqualQuality);

 private:
  struct FrameData {
    WebPPicture pic;
    int timestamp_ms;  // Ending timestamp in milliseconds.
    WebPConfig config;
    size_t encoded_size = 0;
    int final_quality = -1;
    float final_psnr = 0.0;
    bool near_lossless = false;
    // Array containing pairs of (size, psnr) for qualities in range [0..100]
    // when using lossy encoding. If WebPEncode() has not been called for
    // quality 'x', lossy_data['x'] = (-1,-1.0).
    std::pair<int, float> lossy_data[101];

    FrameData(const WebPPicture& pic, int timestamp_ms,
              const WebPConfig& config)
        : pic(pic), timestamp_ms(timestamp_ms), config(config){};
  };
  std::vector<FrameData> frames_;
  WebPAnimEncoder* enc_ = NULL;
  WebPAnimEncoderOptions anim_config_;
  int loop_count_;
  size_t byte_budget_;
  int minimum_lossy_quality_;
  bool verbose_;
  int webp_method_;
  float slope_dPSNR_;

  // Computes the size (in bytes) and PSNR of the 'ind'-th frame. The resulting
  // size and PSNR will be stored in '*pic_size' and '*pic_PSNR' respectively.
  Status GetPictureStats(int ind, size_t* const pic_size,
                         float* const pic_PSNR);

  Status SetLoopCount(WebPData* const webp_data);

  // Generates the animation with given config for each frame.
  Status GenerateAnimationNoBudget(WebPData* const webp_data);

  // Finds the best quality that makes the animation fit right below the given
  // byte budget and generates the animation. The 'webp_data' argument is
  // expected to be initialized (otherwise WebPDataClear() might free some
  // random memory somewhere because the pointer is undefined).
  Status GenerateAnimationEqualQuality(WebPData* const webp_data);

  // Generates the animation so that all frames have similar PSNR (all) values.
  // In case of failure, returns the animation generated by
  // GenerateAnimationEqualQuality().
  Status GenerateAnimationEqualPSNR(WebPData* const webp_data);

  // Encodes frames with near-lossless compression, the near-lossless
  // pre-processing value for each frames can be different. Either
  // GenerateAnimationEqualQuality() or GenerateAnimationEqualPSNR() must be
  // called before to generate animation with lossy encoding. In case of
  // failure, returns the latest lossy encoded frames.
  Status NearLosslessDiff(WebPData* const webp_data);

  // Encodes frames with near-lossless compression using the same pre-processing
  // value for all near-lossless frames. Either GenerateAnimationEqualQuality()
  // or GenerateAnimationEqualPSNR() must be called before to generate animation
  // with lossy encoding. In case of failure, returns the latest lossy encoded
  // frames.
  Status NearLosslessEqual(WebPData* const webp_data);

  // Generates the animation with the slope optimization for the RD-curve.
  // Either lossy and near-lossless compression modes will be used for each
  // frame.
  Status GenerateAnimationSlopeOptim(WebPData* const webp_data);

  // For each frame, finds the leftmost point on the RD-curve (for lossy
  // encoding) so that the difference in PSNR between this point and the last
  // one on the curve (quality = 100) is approximately 1. Then computes the
  // slopes from these points and finds the median one.
  Status FindMedianSlope(float* const slope);

  // Computes the slope between two quality values on the RD-curve when using
  // lossy encoding for the 'ind'-th frame. The result will be stored in
  // '*slope'.
  Status ComputeSlope(int ind, int low_quality, int high_quality,
                      float* const slope);

  // Generates the animation with lossy compression mode and optimizes the
  // resulting qualities using the median slope in order to save byte budget for
  // calling TryNearLossless().
  Status LossyEncodeSlopeOptim(WebPData* const webp_data);

  // Tries to re-encode each frame with the lossy compression mode to find the
  // better PSNR values if possible. Both LossyEncodeSlopeOptim() and
  // TryNearLossless() must be respectively called before to generate the
  // animation.
  Status LossyEncodeNoSlopeOptim(WebPData* const webp_data);

  // Re-encodes frames with lossy compression mode using the unused extra budget
  // from LossyEncodeNoSlopeOptim() in order to get better PSNR values.
  Status ExtraLossyEncode(WebPData* const webp_data);

  // Returns animation size (in bytes).
  size_t GetAnimationSize(WebPData* const webp_data);
};

}  // namespace libwebp

#endif  // THUMBNAILER_SRC_THUMBNAILER_H_
