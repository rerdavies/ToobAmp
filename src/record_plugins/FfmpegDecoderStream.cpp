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
 #include <unistd.h>
 #include <fcntl.h>
 #include <sys/wait.h>
 #include <stdexcept>
 #include <vector>
 #include <string>
 #include <string.h>

 using namespace toob;

 #pragma GCC diagnostic ignored "-Wunused-result" // GCC 12 bug.

 void FfmpegDecoderStream::open(const std::filesystem::path &filePath, int channels, uint32_t sampleRate)
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

    // std::stringstream ss;
    // ss << "/usr/bin/ffmpeg -i " << fileToCmdline(filename)
    //    << " -f f32le -ar " << ((size_t)getRate())
    //    << " -ac " << (isStereo ? 2 : 1)
    //    << " pipe:" << pipeFd[1] << " 2>/dev/null 1>/dev/null";
    // std::string command = ss.str();

    std::vector<std::string> args;
    args.push_back("/usr/bin/ffmpeg");
    args.push_back("-i");
    args.push_back(filePath.string());    
    args.push_back("-f");
    args.push_back("f32le");
    args.push_back("-ar");
    args.push_back(std::to_string((int32_t)sampleRate));
    args.push_back("-ac");
    args.push_back(std::to_string(channels));
    args.push_back("pipe:" + std::to_string(pipeFd[1]));

    std::vector<const char*> cArgv;
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
        try {
            // close all file handles except the pipe.
            int fdlimit = (int)sysconf(_SC_OPEN_MAX);
            for (int i = STDERR_FILENO + 1; i < fdlimit; i++) 
            {
                if (i != pipeFd[1]) {
                    ::close(i);
                }
            }
            // int errorOutput = ::open("/tmp/ffmpeg.txt", O_CREAT | O_WRONLY,0664 );
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
            if (dup2(errorOutput,2) == -1) 
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
                (void)write(2,err,strlen(err));
                (void)write(2,"\n",1);
            }

            // if we get here, execv failed.
            std::string msg = "execv failed.\n";
            (void)write(2,msg.c_str(),msg.length());
            (void)write(2,cArgv[0],strlen(cArgv[0]));

        }
        catch (const std::exception&e)
        {
            std::string msg = e.what();
            (void)write(2,msg.c_str(),msg.length());
            (void)write(2, "\n",1);

        }
        exit(EXIT_FAILURE);
    } else {
        // original process.

        ::close(pipeFd[1]);
        this->pidChild = pid;
        this->pipefd = pipeFd[0];
    }
 } 


 size_t FfmpegDecoderStream::read(float**buffers, size_t count)
 {
    if (pipefd == -1) return 0;
    if (channels == 1) 
    {
        size_t offset = 0;
        while (offset < count) {
            float *data0 = buffers[0] + offset;
            size_t bytesThisTime = (count - offset)*sizeof(float);

            size_t nRead = ::read(this->pipefd, data0,bytesThisTime);

            if (nRead == 0) 
            {
                ::close(this->pipefd);
                pipefd = -1;
                return offset;
            }
            offset += nRead / sizeof(float);
        }
        return count;
    } else  {
        size_t offset = 0;
        float buffer[1024];
        while (offset < count) {
            size_t bytesThisTime = std::min((count - offset)*sizeof(float)*2, sizeof(buffer));

            size_t nRead = ::read(this->pipefd, buffer,bytesThisTime);

            if (nRead == 0) 
            {
                ::close(this->pipefd);
                pipefd = -1;
                return offset;
            }
            size_t ix = 0;
            size_t framesThisTime = nRead/(sizeof(float)*2);
            for (size_t i = 0; i < framesThisTime; ++i)
            {
                size_t outIx = i + offset;
                for (int c = 0; c < this->channels; ++c)
                {
                    buffers[c][outIx] = buffer[ix++];
                }
            }

            offset += nRead / (sizeof(float)*2);
        }
        return count;
    }

}


FfmpegDecoderStream::~FfmpegDecoderStream()
{
    close();
}

void FfmpegDecoderStream::close() {
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
        if (waitid(P_PID, pidChild, &status, WNOHANG | WEXITED) == -1) {
            nanosleep(&timeout, NULL);
            if (waitid(P_PID, pidChild, &status, WNOHANG | WEXITED) == -1) {
                kill(pidChild, SIGKILL);
                waitid(P_PID, pidChild, &status, WEXITED);
            }
        }
        pidChild = -1;
    }   
}

 