/* ****************************************************************
 *
 * Copyright 2026 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/
#ifndef __MEDIA_AUDIOOUTPUTCONVERTER_H
#define __MEDIA_AUDIOOUTPUTCONVERTER_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <tinyalsa/tinyalsa.h>
#include <resample/speex_resampler.h>

namespace media {
namespace stream {

struct pcm_stream_format_s {
	unsigned int channels;
	unsigned int sampleRate;
	enum pcm_format format;
};

typedef struct pcm_stream_format_s pcm_stream_format_t;

class AudioOutputConverter
{
public:
	AudioOutputConverter();
	~AudioOutputConverter();

	bool configure(const pcm_stream_format_t &source, const pcm_stream_format_t &output);
	void reset();
	void release();

	ssize_t convert(const unsigned char *input, size_t inputBytes, const unsigned char **output);
	ssize_t drain(const unsigned char **output);
	size_t getInputBytesForOutput(size_t outputBytes) const;
	size_t getSourceFrameBytes() const;
	size_t getOutputFrameBytes() const;
	bool isConfigured() const;

private:
	bool ensureBuffer(unsigned char **buffer, size_t *capacity, size_t required);
	bool convertToS16(const unsigned char *input, size_t frames);
	ssize_t resample(const int16_t *input, size_t frames, const unsigned char **output);

	pcm_stream_format_t mSource;
	pcm_stream_format_t mOutput;
	SpeexResamplerState *mResampler;
	unsigned char *mInputBuffer;
	size_t mInputBufferSize;
	unsigned char mPendingInput[2 * sizeof(int32_t)];
	size_t mPendingInputBytes;
	unsigned char *mS16Buffer;
	size_t mS16BufferSize;
	unsigned char *mRechannelBuffer;
	size_t mRechannelBufferSize;
	unsigned char *mOutputBuffer;
	size_t mOutputBufferSize;
	bool mConfigured;
};

} // namespace stream
} // namespace media

#endif
