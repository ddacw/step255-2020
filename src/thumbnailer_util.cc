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

#include "thumbnailer_util.h"

namespace libwebp {

ThumbnailerUtil::Status ThumbnailerUtil::AnimData2Pictures(
    WebPData* const webp_data, std::vector<EnclosedWebPPicture>* const pics) {
  std::unique_ptr<WebPAnimDecoder, void (*)(WebPAnimDecoder*)> dec(
      WebPAnimDecoderNew(webp_data, NULL), WebPAnimDecoderDelete);
  if (dec == NULL) {
    std::cerr << "Error parsing image." << std::endl;
    return kMemoryError;
  }

  WebPAnimInfo anim_info;
  if (!WebPAnimDecoderGetInfo(dec.get(), &anim_info)) {
    std::cerr << "Error getting global info about the animation";
    return kGenericError;
  }

  const int width = anim_info.canvas_width;
  const int height = anim_info.canvas_height;

  while (WebPAnimDecoderHasMoreFrames(dec.get())) {
    uint8_t* frame_rgba;
    int timestamp;
    if (!WebPAnimDecoderGetNext(dec.get(), &frame_rgba, &timestamp)) {
      std::cerr << "Error decoding frame." << std::endl;
      return kMemoryError;
    }
    pics->emplace_back(new WebPPicture, WebPPictureFree);
    WebPPicture* pic = pics->back().get();
    if (!WebPPictureInit(pic)) return kMemoryError;
    pic->use_argb = 1;
    pic->width = width;
    pic->height = height;
    if (!WebPPictureImportRGBA(pic, frame_rgba, width * 4)) {
      return kMemoryError;
    }
  }
  return kOk;
}

ThumbnailerUtil::Status ThumbnailerUtil::AnimData2PSNR(
    const std::vector<EnclosedWebPPicture>& original_pics,
    WebPData* const webp_data, ThumbnailStatPSNR* const stats) {
  if (stats == NULL) return kMemoryError;

  std::vector<EnclosedWebPPicture> pics;
  const Status data2pics_error = AnimData2Pictures(webp_data, &pics);
  if (data2pics_error != kOk) return data2pics_error;

  if (pics.size() != original_pics.size()) {
    std::cerr << "Picture count mismatched.";
    return kGenericError;
  }

  const int pic_count = original_pics.size();

  for (int i = 0; i < pic_count; ++i) {
    const EnclosedWebPPicture& orig_pic = original_pics[i];
    const EnclosedWebPPicture& pic = pics[i];
    float distortion_result[5];
    if (!WebPPictureDistortion(orig_pic.get(), pic.get(), 0,
                               distortion_result)) {
      break;
    } else {
      stats->psnr.push_back(distortion_result[4]);  // PSNR-all.
    }
  }

  // Not all frames are processed, as WebPPictureDistortion failed.
  if (stats->psnr.size() != pic_count) return kMemoryError;

  // Record statistics.
  std::vector<float> new_psnr(stats->psnr);
  std::sort(new_psnr.begin(), new_psnr.end());
  stats->min_psnr = new_psnr.front();
  stats->max_psnr = new_psnr.back();
  stats->mean_psnr =
      std::accumulate(new_psnr.begin(), new_psnr.end(), 0.0) / pic_count;
  stats->median_psnr = new_psnr[pic_count / 2];

  return kOk;
}

ThumbnailerUtil::Status ThumbnailerUtil::CompareThumbnail(
    const std::vector<EnclosedWebPPicture>& original_pics,
    WebPData* const webp_data_1, WebPData* const webp_data_2,
    ThumbnailDiffPSNR* const diff) {
  if (original_pics.empty()) {
    std::cerr << "Thumbnail doesn't contain any frames.";
    return kOk;
  }
  if (diff == NULL) return kMemoryError;

  ThumbnailStatPSNR stats_1, stats_2;
  Status error;
  error = AnimData2PSNR(original_pics, webp_data_1, &stats_1);
  if (error != kOk) return error;
  error = AnimData2PSNR(original_pics, webp_data_2, &stats_2);
  if (error != kOk) return error;

  // Both thumbnails now contains the same number of WebPPicture(s)
  // as original_pics.
  const int pic_count = original_pics.size();

  for (int i = 0; i < pic_count; ++i) {
    diff->psnr_diff.push_back(stats_2.psnr[i] - stats_1.psnr[i]);
  }

  // Record statistics.
  std::vector<float> psnr_diff(diff->psnr_diff);
  std::sort(psnr_diff.begin(), psnr_diff.end());
  diff->max_psnr_decrease = psnr_diff.front();
  diff->max_psnr_increase = psnr_diff.back();
  diff->sum_psnr_diff =
      std::accumulate(psnr_diff.begin(), psnr_diff.end(), 0.0);
  diff->mean_psnr_diff = diff->sum_psnr_diff / pic_count;
  diff->median_psnr_diff = psnr_diff[pic_count / 2];

  return kOk;
}

void ThumbnailerUtil::PrintThumbnailStatPSNR(
    const ThumbnailerUtil::ThumbnailStatPSNR& stats) {
  if (stats.psnr.empty()) return;
  std::cerr << "Frame count: " << stats.psnr.size() << '\n';
  std::cerr << std::setw(14) << std::left << "Min PSNR: " << stats.min_psnr
            << '\n';
  std::cerr << std::setw(14) << std::left << "Max PSNR: " << stats.max_psnr
            << '\n';
  std::cerr << std::setw(14) << std::left << "Mean PSNR: " << stats.mean_psnr
            << '\n';
  std::cerr << std::setw(14) << std::left
            << "Median PSNR: " << stats.median_psnr << '\n';
  std::cerr << '\n';
}

void ThumbnailerUtil::PrintThumbnailDiffPSNR(
    const ThumbnailerUtil::ThumbnailDiffPSNR& diff) {
  if (diff.psnr_diff.empty()) return;
  std::cerr << "Frame count: " << diff.psnr_diff.size() << '\n';

  if (diff.max_psnr_decrease > 0) {
    std::cerr << "All frames improved in PSNR.\n";
  } else {
    std::cerr << std::setw(22) << std::left
              << "Max PSNR decrease: " << diff.max_psnr_decrease << '\n';
  }

  if (diff.max_psnr_increase < 0) {
    std::cerr << "All frames worsened in PSNR.\n";
  } else {
    std::cerr << std::setw(22) << std::left
              << "Max PSNR increase: " << diff.max_psnr_increase << '\n';
  }

  std::cerr << std::setw(22) << std::left
            << "Sum of PSNR changes: " << diff.sum_psnr_diff << '\n';
  std::cerr << std::setw(22) << std::left
            << "Mean PSNR change: " << diff.mean_psnr_diff << '\n';
  std::cerr << std::setw(22) << std::left
            << "Median PSNR change: " << diff.median_psnr_diff << '\n';
}

}  // namespace libwebp
