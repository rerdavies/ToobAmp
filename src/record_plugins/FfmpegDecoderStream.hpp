/*
 *   Copyright (c) 2025 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:

 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.

 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#pragma once

#include <cstdint>
#include <filesystem>

namespace toob
{

   // Exec FfMpegExec in order to receive streamed decoded audio.
   class FfmpegDecoderStream
   {
   public:
      ~FfmpegDecoderStream();
      void open(const std::filesystem::path &file, int channels, uint32_t sampleRate);
      size_t read(float**buffers, size_t frames);
      void close();
      bool eof() const { return pipefd == -1; }
   private:
      int channels = 0;
      int pipefd = -1;
      int pidChild = -1;
   };


   class FFMepgMetadata {
   public:
      FFMepgMetadata(const std::filesystem::path &file);
      uint32_t getSampleRate() const { return sampleRate; }
      int getChannels() const { return channels; }
      double getDuration() const { return duration; }  
      const std::string& getTitle() const { return title; }
      int32_t getTrack() const { return track; }
      int32_t getNumberOfTracks() const { return numberOfTracks; }
      const std::string &getAlbum() const { return album; }
      const std::string &getArtist() const { return artist; }

   private:
      uint32_t sampleRate = 0;
      int channels = 0;
      double duration = 0;
      std::string title;
      int32_t track = 0;
      int32_t numberOfTracks = 0;
      std::string album;
      std::string artist;

   };

} // namespace toob