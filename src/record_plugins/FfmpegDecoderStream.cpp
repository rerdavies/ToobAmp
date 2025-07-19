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

// thumbname:

//  ffmpeg -i 01\ Once\ in\ Royal\ David\'s\ City.mp3 -filter:v scale=-2:250 -an output.jpeg

#include "FfmpegDecoderStream.hpp"
#include "../ss.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdexcept>
#include <vector>
#include <string>
#include <string.h>
#include <iostream>
#include "../json.hpp"
#include "../json_variant.hpp"
#include "../LRUCache.hpp"
#include <mutex>

namespace fs = std::filesystem;

using namespace toob;
using namespace pipedal;

#pragma GCC diagnostic ignored "-Wunused-result" // GCC 12 bug.

void FfmpegDecoderStream::open(const std::filesystem::path &filePath, int channels, uint32_t sampleRate, double seekPosSeconds)
{
    this->channels = channels;

    // Requirements: fork the ffmpeg process, making sure that NO file handles (especially socket handlers)
    // are passed to the child process. The one socket handle that is passed in is the return pipe handle.
    // Standard I/O is redirected to /dev/null.
    int pipeFd[2];
    pipeFd[0] = -1;
    pipeFd[1] = -1;

    if (pipe(pipeFd) == -1)
    {
        throw std::runtime_error("Failed to create decoder pipe.");
    }
    // FORK THE FFMPEG decoder process.

    std::vector<std::string> args;
    args.push_back("/usr/bin/ffmpeg");
    args.push_back("-i");
    args.push_back(filePath.string());

    // seek to the position in the file.
    args.push_back("-ss");
    args.push_back(std::to_string(seekPosSeconds));

    args.push_back("-f");
    args.push_back("f32le");
    args.push_back("-acodec");
    args.push_back("pcm_f32le");
    args.push_back("-ac");
    args.push_back(std::to_string(channels));
    args.push_back("-ar");
    args.push_back(std::to_string((int32_t)sampleRate));
    args.push_back("pipe:" + std::to_string(pipeFd[1]));


    std::vector<const char *> cArgv;
    for (auto &arg : args)
    {
        cArgv.push_back(arg.c_str());
    }
    cArgv.push_back(nullptr);

    int pid = fork();

    if (pid == -1)
    {
        throw std::runtime_error("Failed to fork ffmpeg process.");
    }
    if (pid == 0)
    {
        // child process.
        try
        {
            // close all file handles except the pipe.
            int fdlimit = (int)sysconf(_SC_OPEN_MAX);
            for (int i = STDERR_FILENO + 1; i < fdlimit; i++)
            {
                if (i != pipeFd[1])
                {
                    ::close(i);
                }
            }
            // int errorOutput = dup(STDERR_FILENO);
            // int errorOutput = ::open("/tmp/ffmpeg.txt", O_CREAT | O_WRONLY, 0664);
            int errorOutput = ::open("/dev/null", O_WRONLY);

            // Put /dev/null into stdin, stdout, stderr.
            int devnullr = ::open("/dev/null", O_RDONLY);
            if (devnullr == -1)
            {
                throw std::runtime_error("Failed to open /dev/null.");
            }
            int devnullw = ::open("/dev/null", O_WRONLY);
            if (devnullw == -1)
            {
                throw std::runtime_error("Failed to open /dev/null.");
            }
            if (dup2(devnullr, 0) == -1)
            {
                throw std::runtime_error("Failed to dup2 /dev/null to stdin.");
            }
            if (dup2(devnullw, 1) == -1)
            {
                throw std::runtime_error("Failed to dup2 /dev/null to stdout.");
            }
            if (dup2(errorOutput, 2) == -1)
            {
                throw std::runtime_error("Failed to dup2 /dev/null to stderr.");
            }
            ::close(errorOutput);

            // if (dup2(devnullw, 2) == -1)
            // {
            //     throw std::runtime_error("Failed to dup2 /dev/null to stderr.");
            // }
            ::close(devnullw);
            ::close(devnullr);

            int rc = execv(cArgv[0], (char *const *)&cArgv[0]);
            if (rc == -1)
            {
                const char *err = strerror(errno);
                (void)write(2, err, strlen(err));
                (void)write(2, "\n", 1);
            }

            // if we get here, execv failed.
            std::string msg = "execv failed.\n";
            (void)write(2, msg.c_str(), msg.length());
            (void)write(2, cArgv[0], strlen(cArgv[0]));
        }
        catch (const std::exception &e)
        {
            std::string msg = e.what();
            (void)write(2, msg.c_str(), msg.length());
            (void)write(2, "\n", 1);
        }
        exit(EXIT_FAILURE);
    }
    else
    {
        // original process.

        ::close(pipeFd[1]);
        this->pidChild = pid;
        this->pipefd = pipeFd[0];
    }
}

size_t FfmpegDecoderStream::read(float **buffers, size_t count)
{
    if (pipefd == -1)
        return 0;
    if (channels == 1)
    {
        size_t offset = 0;
        while (offset < count)
        {
            float *data0 = buffers[0] + offset;
            size_t bytesThisTime = (count - offset) * sizeof(float);

            size_t nRead = ::read(this->pipefd, data0, bytesThisTime);

            if (nRead == 0)
            {
                ::close(this->pipefd);
                pipefd = -1;
                return offset;
            }
            offset += nRead / sizeof(float);
        }
        return count;
    }
    else
    {
        size_t offset = 0;
        float buffer[1024];
        while (offset < count)
        {
            size_t bytesThisTime = std::min((count - offset) * sizeof(float) * 2, sizeof(buffer));

            size_t nRead = ::read(this->pipefd, buffer, bytesThisTime);

            if (nRead == 0)
            {
                ::close(this->pipefd);
                pipefd = -1;
                return offset;
            }
            size_t ix = 0;
            size_t framesThisTime = nRead / (sizeof(float) * 2);
            for (size_t i = 0; i < framesThisTime; ++i)
            {
                size_t outIx = i + offset;
                for (int c = 0; c < this->channels; ++c)
                {
                    buffers[c][outIx] = buffer[ix++];
                }
            }

            offset += nRead / (sizeof(float) * 2);
        }
        return count;
    }
}

FfmpegDecoderStream::~FfmpegDecoderStream()
{
    close();
}

void FfmpegDecoderStream::close()
{
    if (pipefd != -1)
    {
        ::close(pipefd);
        pipefd = -1;
    }
    if (pidChild != -1)
    {
        // wait for child process to exit (with timeout)

        kill(pidChild, SIGINT);

        siginfo_t status;
        struct timespec timeout = {.tv_sec = 0, .tv_nsec = 20000000};
        if (waitid(P_PID, pidChild, &status, WNOHANG | WEXITED) == -1)
        {
            nanosleep(&timeout, NULL);
            if (waitid(P_PID, pidChild, &status, WNOHANG | WEXITED) == -1)
            {
                kill(pidChild, SIGKILL);
                waitid(P_PID, pidChild, &status, WEXITED);
            }
        }
        pidChild = -1;
    }
}

/*
ffprobe -loglevel error -show_streams -show_format -print_format stream_tags -of json ~/Music/FLAC/Dexter\ Gordon\ -\ Ballads\ \[FLAC-1995]/01\ -\ Darn\ That\ Dream.flac
{
    "streams": [
        {
            "index": 0,
            "codec_name": "flac",
            "codec_long_name": "FLAC (Free Lossless Audio Codec)",
            "codec_type": "audio",
            "codec_tag_string": "[0][0][0][0]",
            "codec_tag": "0x0000",
            "sample_fmt": "s16",
            "sample_rate": "44100",
            "channels": 2,
            "channel_layout": "stereo",
            "bits_per_sample": 0,
            "initial_padding": 0,
            "r_frame_rate": "0/0",
            "avg_frame_rate": "0/0",
            "time_base": "1/44100",
            "start_pts": 0,
            "start_time": "0.000000",
            "duration_ts": 19947900,
            "duration": "452.333333",
            "bits_per_raw_sample": "16",
            "extradata_size": 34,
            "disposition": {
                "default": 0,
                "dub": 0,
                "original": 0,
                "comment": 0,
                "lyrics": 0,
                "karaoke": 0,
                "forced": 0,
                "hearing_impaired": 0,
                "visual_impaired": 0,
                "clean_effects": 0,
                "attached_pic": 0,
                "timed_thumbnails": 0,
                "non_diegetic": 0,
                "captions": 0,
                "descriptions": 0,
                "metadata": 0,
                "dependent": 0,
                "still_image": 0
            }
        },
        {
            "index": 1,
            "codec_name": "mjpeg",
            "codec_long_name": "Motion JPEG",
            "profile": "Baseline",
            "codec_type": "video",
            "codec_tag_string": "[0][0][0][0]",
            "codec_tag": "0x0000",
            "width": 2814,
            "height": 2818,
            "coded_width": 2814,
            "coded_height": 2818,
            "closed_captions": 0,
            "film_grain": 0,
            "has_b_frames": 0,
            "sample_aspect_ratio": "1:1",
            "display_aspect_ratio": "1407:1409",
            "pix_fmt": "yuvj420p",
            "level": -99,
            "color_range": "pc",
            "color_space": "bt470bg",
            "chroma_location": "center",
            "refs": 1,
            "r_frame_rate": "90000/1",
            "avg_frame_rate": "0/0",
            "time_base": "1/90000",
            "start_pts": 0,
            "start_time": "0.000000",
            "duration_ts": 40710000,
            "duration": "452.333333",
            "bits_per_raw_sample": "8",
            "disposition": {
                "default": 0,
                "dub": 0,
                "original": 0,
                "comment": 0,
                "lyrics": 0,
                "karaoke": 0,
                "forced": 0,
                "hearing_impaired": 0,
                "visual_impaired": 0,
                "clean_effects": 0,
                "attached_pic": 1,
                "timed_thumbnails": 0,
                "non_diegetic": 0,
                "captions": 0,
                "descriptions": 0,
                "metadata": 0,
                "dependent": 0,
                "still_image": 0
            },
            "tags": {
                "comment": "Cover (front)"
            }
        }
    ],
    "format": {
        "filename": "/home/robin/Music/FLAC/Dexter Gordon - Ballads [FLAC-1995]/01 - Darn That Dream.flac",
        "nb_streams": 2,
        "nb_programs": 0,
        "format_name": "flac",
        "format_long_name": "raw FLAC",
        "start_time": "0.000000",
        "duration": "452.333333",
        "size": "43638755",
        "bit_rate": "771798",
        "probe_score": 100,
        "tags": {
            "ARTIST": "Dexter Gordon",
            "TITLE": "Darn That Dream",
            "ALBUM": "Ballads",
            "DATE": "1978",
            "track": "01",
            "GENRE": "Jazz",
            "COMMENT": "Exact Audio Copy - trfkad"
        }
    }
}


ffmpeg -loglevel error -i "/home/robin/Music/FLAC/Dexter Gordon - Ballads [FLAC-1995]/01 - Darn That Dream.flac" --an -vf scale=200:200 /tmp/thumb-%03d.jpg




*/
// Safely encode a filename for use in a shell command
static std::string shell_escape_filename(const std::string &filename)
{
    std::string escaped;
    escaped.reserve(filename.size() + 2); // Reserve space for quotes and content

    // Wrap the filename in single quotes
    escaped += '\'';

    for (char c : filename)
    {
        // Escape single quotes by closing the quote, adding escaped quote, and reopening
        if (c == '\'')
        {
            escaped += "'\\''";
        }
        else
        {
            escaped += c;
        }
    }

    escaped += '\'';
    return escaped;
}

static std::string readPipeToString(FILE *file)
{
    std::string content;
    char buffer[4096];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        content.append(buffer, bytesRead);
    }
    return content;
}
static std::string GetJsonMetadata(const std::filesystem::path &path)
{
    /* ffprobe -loglevel error -show_streams -show_format -print_format stream_tags -of json
           ~/filename.flac */

    std::stringstream ss;
    ss << "/usr/bin/ffprobe -loglevel error -show_streams -show_format -print_format stream_tags -of json  "
       << shell_escape_filename(path.string())
       << " 2>/dev/null";

    FILE *output = popen(ss.str().c_str(), "r");
    if (output == nullptr)
    {
        throw std::runtime_error("Failed to process file.");
    }
    std::string result;
    try
    {
        result = readPipeToString(output);
    }
    catch (const std::exception &e)
    {
        pclose(output);
        throw std::runtime_error(e.what());
    }
    int retcode = pclose(output);
    if (retcode != 0)
    {
        throw std::runtime_error("Invalid file format.");
    }
    return result;
}

// static float MetadataFloat(const json_variant &vt, float defaultValue = 0)
// {
//     if (vt.is_string())
//     {
//         std::string s = vt.as_string();
//         std::stringstream ss(s);
//         float result = defaultValue;
//         ss >> result;
//         return result;
//     }
//     return defaultValue;
// }
static double MetadataDouble(const json_variant &vt, double defaultValue = 0)
{
    if (vt.is_string())
    {
        std::string s = vt.as_string();
        std::stringstream ss(s);
        double result = defaultValue;
        ss >> result;
        return result;
    }
    return defaultValue;
}

static std::string MetadataString(const json_variant &o, const std::vector<std::string> &names)
{
    for (const auto &name : names)
    {
        auto result = (*o.as_object())[name];
        if (result.is_string())
        {
            return result.as_string();
        }
    }
    return "";
}

AudioFileMetadata::AudioFileMetadata(const std::filesystem::path &file)
{
    const std::string json = GetJsonMetadata(file);

    json_variant vt;

    std::stringstream ss(json);
    json_reader reader(ss);

    try {
        reader.read(&vt);
    } catch (const std::exception &e) {
        throw std::runtime_error("Invalid metadata.");
    }
    if (!vt.is_object())
    {
        throw std::runtime_error("Invalid metadata format for file: " + file.string());
    }

    auto top = vt.as_object();
    auto format = top->at("format").as_object();

    this->path = file;
    try {
        this->duration = MetadataDouble(format->at("duration"), 0);
    } catch (const std::exception &e) {
        throw std::runtime_error("Invalid file format.");
    }
    if (this->duration <= 0)
    {
        throw std::runtime_error("Invalid duration in metadata for file: " + file.string());
    }

    try {
        auto tags = (*format)["tags"];
        if (tags.is_object())
        {
            this->album = MetadataString(tags, {"ALBUM", "album"});
            this->artist = MetadataString(tags, {"ARTIST", "artist"});
            this->album_artist = MetadataString(tags, {"ALBUM ARTIST", "album_artist", "album artist"});
            this->title = MetadataString(tags, {"TITLE", "title"});
            this->date = MetadataString(tags, {"DATE", "date"});
            this->year = MetadataString(tags, {"YEAR", "year"});
            this->track = MetadataString(tags, {
                                                "track",
                                                "TRACK",
                                            });
            this->disc = MetadataString(tags, {"disc", "DISC"});

            this->totalTracks = MetadataString(tags, {"TOTALTRACKS"});
        }
    } catch (const std::exception &e) {
        (void)e;
        // ignored.
    }
}

/*
ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 ~/Music/FLAC/St\ Germain/St\ Germain/04\ -\ Voila.flac
*/

namespace
{
    class AudioCacheKey
    {
    public:
        AudioCacheKey(const std::filesystem::path &path)
        {
            this->path = path.string();
            lastWrite = std::filesystem::last_write_time(path);
        }
        // Equality operator
        bool operator==(const AudioCacheKey &other) const
        {
            return path == other.path && lastWrite == other.lastWrite;
        }

    public:
        std::string path;
        fs::file_time_type lastWrite;
    };
}

namespace std
{
    template <>
    struct hash<AudioCacheKey>
    {

        size_t operator()(const AudioCacheKey &key) const
        {
            size_t path_hash = hash<std::string>{}(key.path);
            size_t time_hash = hash<uintmax_t>{}(
                duration_cast<std::chrono::nanoseconds>(key.lastWrite.time_since_epoch()).count());
            // Combine hashes (boost::hash_combine approach)
            return path_hash ^ (time_hash + 0x9e3779b9 + (path_hash << 6) + (path_hash >> 2));
        }
    };
}

static LRUCache<AudioCacheKey, AudioFileMetadata> metadataCache{100};
static std::mutex metadataCacheMutex;

AudioFileMetadata toob::GetAudioFileMetadata(const std::filesystem::path &path)
{
    
    std::lock_guard lock{metadataCacheMutex};
    AudioCacheKey key{path};
    AudioFileMetadata result;
    if (metadataCache.get(key, result))
    {
        return result;
    }
    result = AudioFileMetadata(path);
    metadataCache.put(key, result);
    return result;
}

double toob::GetAudioFileDuration(const std::filesystem::path &path)
{
    AudioFileMetadata md = GetAudioFileMetadata(path);
    return md.getDuration();
}

// ffmpeg -i 01\ Eighty-One\ \(1\).m4a -c copy  -metadata title="xxx"   tmp.m4a
