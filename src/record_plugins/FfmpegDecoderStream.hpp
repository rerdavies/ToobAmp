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
#include <vector>

namespace toob
{

    // Exec FfMpegExec in order to receive streamed decoded audio.
    class FfmpegDecoderStream
    {
    public:
        ~FfmpegDecoderStream();
        void open(const std::filesystem::path &file, int channels, uint32_t sampleRate, double seekPosSeconds = 0.0);
        size_t read(float **buffers, size_t frames);
        void close();
        bool eof() const { return pipefd == -1; }

    private:
        int channels = 0;
        int pipefd = -1;
        int pidChild = -1;
    };

    class AudioFileMetadata;
    class ThumbnailMetadata
    {

    public:
        size_t getWidth() { return width; }
        size_t getHeight() { return height; }

    private:
        friend class AudioFileMetadata;
        size_t width;
        size_t height;
    };


    double GetAudioFileDuration(const std::filesystem::path &file);
    
    class AudioFileMetadata
    {
    public:
        AudioFileMetadata() { }
        AudioFileMetadata(const std::filesystem::path &file);
        AudioFileMetadata(const AudioFileMetadata&other) = default;

        //AudioFileMetadata&operator=(const AudioFileMetadata&) = default;
        const std::filesystem::path &getPath() { return path; }
        double getDuration() const { return duration; }
        const std::string &getTitle() const { return title; }
        const std::string& getTrack() const { return track; }
        const std::string& getTotalTracks() const { return totalTracks; }
        const std::string &getAlbum() const { return album; }
        const std::string &getDisc() const { return disc; }
        const std::string &getArtist() const { return artist; }
        const std::string &getAlbumArtist() const { return artist; }

    private:
        std::filesystem::path path;
        double duration = 0;
        std::string artist;
        std::string album_artist;
        std::string title;
        std::string album;
        std::string date;
        std::string year;

        std::string track;
        std::string disc;
        std::string totalTracks;
    };

    AudioFileMetadata GetAudioFileMetadata(const std::filesystem::path &path);

} // namespace toob