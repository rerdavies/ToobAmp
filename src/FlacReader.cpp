/*
 *   Copyright (c) 2022 Robin E. R. Davies
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

#include "FlacReader.hpp"
#include <FLAC++/decoder.h>
#include <stdexcept>
#include "ss.hpp"

using namespace FLAC;
using namespace FLAC::Decoder;

namespace toob
{

    class FlacDecoder : public FLAC::Decoder::File
    {
    public:
        FlacDecoder(AudioData &audioData)
            : FLAC::Decoder::File(), audioData(audioData)
        {
        }
        std::string getErrorMessage() const {
            return errorMessage;
        }
        size_t getLength() const { return this->sampleOffset; }
    protected:

        virtual ::FLAC__StreamDecoderLengthStatus length_callback(	FLAC__uint64 * 	stream_length	)	
        {
            return FLAC__StreamDecoderLengthStatus::FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
        }

        virtual ::FLAC__StreamDecoderWriteStatus write_callback(const ::FLAC__Frame *frame, const FLAC__int32 *const buffers[]) override
        {
            if (!seenStreamInfo)
            {
                errorMessage = "Received data before receiving stream format.";
                return FLAC__StreamDecoderWriteStatus::FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
            }
            size_t i;

            // Update data
            size_t channels = audioData.getChannelCount();

            if (frame->header.blocksize + sampleOffset > audioData.getSize())
            {
                size_t newSize = (audioData.getSize() + frame->header.blocksize)*3/2;
                if (newSize < 64*1024)
                {
                    newSize = 64*1024;
                }
                audioData.setSize(newSize);
            }

            /* write decoded PCM samples */

            auto &outputBuffers = audioData.getData();
            if (this->bitsPerSample == 16)
            {
                static constexpr float SCALE = 1.0f/32768;

                for (size_t c = 0; c < channels; ++c)
                {
                    auto &outputBuffer = outputBuffers[c];
                    auto buffer = buffers[c];
                    for (i = 0; i < frame->header.blocksize; i++)
                    {
                            outputBuffer[sampleOffset+i] = SCALE*(FLAC__int16)buffer[i];
                    }
                }
                sampleOffset += frame->header.blocksize;
            } else if (this->bitsPerSample == 24)
            {
                static constexpr float SCALE = 1.0f/(32768L*256L);

                for (size_t c = 0; c < channels; ++c)
                {
                    auto &outputBuffer = outputBuffers[c];
                    auto buffer = buffers[c];
                    for (i = 0; i < frame->header.blocksize; i++)
                    {
                            outputBuffer[sampleOffset+i] = SCALE*(FLAC__int32)buffer[i];
                    }
                }
                sampleOffset += frame->header.blocksize;

            } else if (this->bitsPerSample == 32)
            {
                static constexpr float SCALE = 1.0f/(32768L*65536L);

                for (size_t c = 0; c < channels; ++c)
                {
                    auto &outputBuffer = outputBuffers[c];
                    auto buffer = buffers[c];
                    for (i = 0; i < frame->header.blocksize; i++)
                    {
                            outputBuffer[sampleOffset+i] = SCALE*(FLAC__int32)buffer[i];
                    }
                }
                sampleOffset += frame->header.blocksize;

            } else {
                this->errorMessage = "Invalid bits per sample.";
                return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
            }
            return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
        }

        virtual bool eof_callback() override
        {
            audioData.setSize(sampleOffset); // trim the buffers to correct size.
            return true;
        }

        virtual void metadata_callback(const ::FLAC__StreamMetadata *metadata) override
        {
            if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
            {
                seenStreamInfo = true;

                const FLAC__StreamMetadata_StreamInfo &stream_info = metadata->data.stream_info;
                audioData.setSampleRate((size_t)stream_info.sample_rate);
                audioData.setChannelCount(stream_info.channels);
                audioData.setSize(stream_info.total_samples);
                this->bitsPerSample = stream_info.bits_per_sample;
            }
        }
        virtual void error_callback(::FLAC__StreamDecoderErrorStatus status)
        {
            this->errorMessage = "Invalid file format.";
        }

    private:
        FlacDecoder(const FlacDecoder &) = delete;
        FlacDecoder &operator=(const FlacDecoder &) = delete;

        bool seenStreamInfo = false;
        float bitsPerSample = 0;
        std::string errorMessage;
        size_t sampleOffset = 0;

        AudioData &audioData;
    };

    /*static*/ AudioData FlacReader::Load(const std::filesystem::path &path)
    {
        AudioData result;
        FlacDecoder decoder{result};
        FLAC__StreamDecoderInitStatus rc = decoder.init(path);
        if (rc != FLAC__STREAM_DECODER_INIT_STATUS_OK)
        {
            if (rc == FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE)
            {
                throw std::logic_error(SS("Can't open file " << path));
            }
            else {
                throw std::logic_error(SS("Invalid file format: " << path));
            }
        }
        if (!decoder.process_until_end_of_stream())
        {
            std::string message = decoder.getErrorMessage();
            if (message == "" )
            {
                message = SS("Invalid file format: " << path);
                throw std::logic_error(message);
            }
        }
        result.setSize(decoder.getLength());
        return result;
        
    }
}
