// Copyright 2019 Google LLC
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

#include <fstream>
#include <iostream>
#include <string>

#include "class.h"

// Returns true on success or false on failure.
static bool ReadImage(const char filename[], WebPPicture* const pic) {
  const uint8_t* data = NULL;
  size_t data_size = 0;
  if (!ImgIoUtilReadFile(filename, &data, &data_size)) return false;

  WebPImageReader reader = WebPGuessImageReader(data, data_size);
  bool ok = reader(data, data_size, pic, 1, NULL);
  free((void*)data);

  return ok;
}

int main(int argc, char* argv[]) {
  libwebp::Thumbnailer thumbnailer = libwebp::Thumbnailer();

  // Process list of images and timestamps.
  std::vector<std::unique_ptr<WebPPicture, void (*)(WebPPicture*)>> pics;
  std::ifstream input_list(argv[1]);
  std::string filename_str;
  int timestamp_ms;

  while (input_list >> filename_str >> timestamp_ms) {
    pics.emplace_back(new WebPPicture, WebPPictureFree);
    WebPPictureInit(pics.back().get());

    // convert std::string to char*
    char filename[filename_str.length()];
    strcpy(filename, filename_str.c_str());

    ReadImage(filename, pics.back().get());
    thumbnailer.AddFrame(*pics.back().get(), timestamp_ms);
  }

  // Write animation to file.
  const char* output = argv[2];
  WebPData webp_data;
  WebPDataInit(&webp_data);
  thumbnailer.GenerateAnimation(&webp_data);
  ImgIoUtilWriteFile(output, webp_data.bytes, webp_data.size);
  WebPDataClear(&webp_data);

  return 0;
}