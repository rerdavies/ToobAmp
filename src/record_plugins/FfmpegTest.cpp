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

#include "FfmpegDecoderStream.hpp"
#include <exception>
#include <iostream>
#include <set>
#include <chrono>
#include <vector>
#include <cmath>

using namespace toob;
namespace fs = std::filesystem;
namespace chrono = std::chrono;
using namespace std;

static std::set<std::string> AUDIO_EXTENSIONS = {
    ".mp3", ".mp4", ".flac", ".wav"};
static bool isAudioFile(const std::filesystem::path &path)
{
    std::string extension = path.extension().string();
    return AUDIO_EXTENSIONS.contains(extension);
}

void ReadMetadataFromMusicDirectory()
{
    std::cout
        << "Reading data from all files in ~/Music" << std::endl;
    auto start = chrono::high_resolution_clock::now();
    size_t nFiles = 0;
    try
    {
        auto homeDir = getenv("HOME");
        std::filesystem::path path = fs::path(homeDir) / "Music";

        for (const auto &entry : std::filesystem::recursive_directory_iterator(path))
        {
            if (entry.is_regular_file())
            {
                auto path = entry.path();
                auto extension = path.extension().string();
                if (isAudioFile(path))
                {
                    ++nFiles;
                    try
                    {
                        AudioFileMetadata md(path);

                        if (md.getDuration() == 0)
                        {
                            throw std::runtime_error("Duration is zero.");
                        }
                        // cout << path << endl;
                        // cout << " (" << md.getTrack()
                        //      << "/"
                        //      << md.getTotalTracks()
                        //      << " "
                        //      << md.getAlbum()
                        //      << ") "
                        //      << md.getTitle()
                        //      << "/"
                        //      << md.getArtist()
                        //      << " ["
                        //      << md.getDuration()
                        //      << "]"
                        //      << endl;
                    }
                    catch (const std::exception &e)
                    {
                        cout << "   ERROR: " << e.what() << " - " << path << endl;
                    }
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        cerr << "ERROR: " << e.what() << endl;
        exit(EXIT_FAILURE);
    }
    cout << nFiles << " read." << endl;
    std::chrono::duration<double> elapsed = chrono::high_resolution_clock::now() - start;
    cout << "Elapsed: " << elapsed.count() << endl;
    cout << (elapsed.count() / nFiles) << " per file.";
}

void ReadDurationsFromMusicDirectory()
{
    size_t nFiles = 0;
    std::cout
        << "Reading durations from all files in ~/Music" << std::endl;
    auto start = chrono::high_resolution_clock::now();
    try
    {

        auto homeDir = getenv("HOME");
        std::filesystem::path path = fs::path(homeDir) / "Music";

        for (const auto &entry : std::filesystem::recursive_directory_iterator(path))
        {
            if (entry.is_regular_file())
            {
                auto path = entry.path();
                auto extension = path.extension().string();
                if (isAudioFile(path))
                {
                    try
                    {
                        ++nFiles;
                        double duration = GetAudioFileDuration(path);

                        if (duration == 0)
                        {
                            throw std::runtime_error("Duration is zero.");
                        }
                        // cout << path << endl;
                        // cout << " (" << md.getTrack()
                        //      << "/"
                        //      << md.getTotalTracks()
                        //      << " "
                        //      << md.getAlbum()
                        //      << ") "
                        //      << md.getTitle()
                        //      << "/"
                        //      << md.getArtist()
                        //      << " ["
                        //      << md.getDuration()
                        //      << "]"
                        //      << endl;

                        size_t nSamples = 0;
                        {
                            // read the file to see whether the duration is correct
                            FfmpegDecoderStream decoder;
                            decoder.open(path, 1, 48000, 0.0f);
                            float buffer[1024];
                            float *buffers[2] = {buffer, nullptr};
                            while (!decoder.eof())
                            {
                                size_t n = decoder.read(buffers, 1024);
                                if (n == 0)
                                {
                                    break; // no more data to read.
                                }
                                nSamples += n;
                            }
                            decoder.close();
                        }
                        size_t nExpectedSamples = (size_t)std::round(duration * 48000.0f);
                        if (nSamples != nExpectedSamples)
                        {
                            int64_t difference = int64_t(nSamples) - int64_t(nExpectedSamples);
                            cout << "    "
                                 << "Duration mismatch: << " 
                                 <<  (difference > 0 ? "+":"") << difference 
                                 << " Expected: " << nExpectedSamples <<
                                    " Got: "
                                 << nSamples << endl
                                 << "       file: " + path.string() << endl;
                        }
                    }
                    catch (const std::exception &e)
                    {
                        cout << "   ERROR: " << e.what() << " - " << path << endl;
                    }
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        cerr << "ERROR: " << e.what() << endl;
        exit(EXIT_FAILURE);
    }
    cout << nFiles << " read." << endl;
    std::chrono::duration<double> elapsed = chrono::high_resolution_clock::now() - start;
    cout << "Elapsed: " << elapsed.count() << endl;
    cout << (elapsed.count() / nFiles) << " per file.";
}

void CheckSeekPrecision(const std::string &testFile)
{
    std::vector<float> reference;

    try
    {
        FfmpegDecoderStream decoder;

        auto path = std::filesystem::current_path() / testFile;
        decoder.open(path, 1, 48000, 0.0f);

        float buffer[1024];
        float *buffers[2] = {buffer, nullptr};

        while (!decoder.eof())

        {

            size_t nRead = decoder.read(buffers, 1024);
            if (nRead > 0)
            {
                reference.insert(reference.end(), buffer, buffer + nRead);
            }
            else
            {
                break;
            }
        }
        decoder.close();
    }
    catch (const std::exception &e)
    {
        cerr << "ERROR: " << e.what() << endl;
        exit(EXIT_FAILURE);
    }

    for (size_t seekOffset : std::vector<size_t>{24031, 10, 2500, 20, 3513})
    {
        try
        {
            FfmpegDecoderStream decoder;

            decoder.open(testFile, 1, 48000, (double)seekOffset / 48000.0, true);

            float buffer[1024];
            float *buffers[2] = {buffer, nullptr};

            size_t ix = seekOffset;
            while (true)
            {
                size_t nRead = decoder.read(buffers, 1024);
                if (nRead > 0)
                {
                    for (size_t i = 0; i < nRead; ++i)
                    {
                        if (reference[ix] != buffer[i])
                        {
                            throw std::runtime_error(
                                "Seek precision error at offset " + std::to_string(ix + seekOffset) +
                                ": expected " + std::to_string(reference[ix + seekOffset]) +
                                ", got " + std::to_string(buffer[i]));
                        }
                        ++ix;
                    }
                }
                else
                {
                    break;
                }
            }
            decoder.close();
        }
        catch (const std::exception &e)
        {
            cerr << "ERROR: " << e.what() << endl;
        }
    }
}

void CheckSeekPrecision()
{
    cout << "Checking seek precision for looped files..." << endl;

    std::vector<std::string> testFiles = {
        "Assets/LoopTest/LoopData.mp3",
        "Assets/LoopTest/LoopData.flac",
        "Assets/LoopTest/LoopData.m4a"};

    for (const auto &testFile : testFiles)
    {
        CheckSeekPrecision(testFile);
    }
}

int main(int argc, char **argv)
{

    ReadDurationsFromMusicDirectory();
    CheckSeekPrecision();
    ReadMetadataFromMusicDirectory();
    return EXIT_SUCCESS;
}
