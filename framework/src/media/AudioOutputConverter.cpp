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

#include <debug.h>
#include <limits.h>
#include <new>
#include <string.h>

#include "AudioOutputConverter.h"
#include "utils/remix.h"

#define AUDIO_OUTPUT_RESAMPLER_QUALITY 5
#define AUDIO_OUTPUT_RESAMPLER_MAX_QUALITY 10

namespace media {
namespace stream {

AudioOutputConverter::AudioOutputConverter() :
	mSource{0, 0, PCM_FORMAT_NONE},
	mOutput{0, 0, PCM_FORMAT_NONE},
	mResampler(nullptr),
	mInputBuffer(nullptr),
	mInputBufferSize(0),
	mPendingInputBytes(0),
	mS16Buffer(nullptr),
	mS16BufferSize(0),
	mRechannelBuffer(nullptr),
	mRechannelBufferSize(0),
	mOutputBuffer(nullptr),
	mOutputBufferSize(0),
	mConfigured(false)
{
}

AudioOutputConverter::~AudioOutputConverter()
{
	release();
}

bool AudioOutputConverter::configure(const pcm_stream_format_t &source, const pcm_stream_format_t &output)
{
	release();

	if (source.channels == 0 || source.channels > 2 || source.sampleRate == 0 ||
		output.channels == 0 || output.channels > 2 || output.sampleRate == 0) {
		meddbg("Invalid PCM channel or sample rate\n");
		return false;
	}

	if (output.format != PCM_FORMAT_S16_LE) {
		meddbg("Only S16_LE hardware output is supported, format: %d\n", output.format);
		return false;
	}

	if (source.format != PCM_FORMAT_S8 && source.format != PCM_FORMAT_S16_LE &&
		source.format != PCM_FORMAT_S32_LE) {
		meddbg("Unsupported source PCM format: %d\n", source.format);
		return false;
	}

	mSource = source;
	mOutput = output;

	if (mSource.sampleRate != mOutput.sampleRate) {
		int error = RESAMPLER_ERR_SUCCESS;
		int quality = AUDIO_OUTPUT_RESAMPLER_QUALITY;
		if ((mOutput.sampleRate > mSource.sampleRate && mOutput.sampleRate % mSource.sampleRate == 0) ||
			(mSource.sampleRate > mOutput.sampleRate && mSource.sampleRate % mOutput.sampleRate == 0)) {
			quality = AUDIO_OUTPUT_RESAMPLER_MAX_QUALITY;
		}

		mResampler = speex_resampler_init(mOutput.channels, mSource.sampleRate,
										mOutput.sampleRate, quality, &error);
		if (!mResampler) {
			meddbg("Failed to create output resampler, error: %d\n", error);
			release();
			return false;
		}
	}

	mConfigured = true;
	return true;
}

void AudioOutputConverter::reset()
{
	mPendingInputBytes = 0;
	if (mResampler) {
		int ret = speex_resampler_reset_mem(mResampler);
		if (ret != RESAMPLER_ERR_SUCCESS) {
			meddbg("Failed to reset output resampler, error: %d\n", ret);
		}
	}
}

void AudioOutputConverter::release()
{
	if (mResampler) {
		speex_resampler_destroy(mResampler);
		mResampler = nullptr;
	}
	delete[] mInputBuffer;
	mInputBuffer = nullptr;
	mInputBufferSize = 0;
	mPendingInputBytes = 0;
	delete[] mS16Buffer;
	mS16Buffer = nullptr;
	mS16BufferSize = 0;
	delete[] mRechannelBuffer;
	mRechannelBuffer = nullptr;
	mRechannelBufferSize = 0;
	delete[] mOutputBuffer;
	mOutputBuffer = nullptr;
	mOutputBufferSize = 0;
	mConfigured = false;
}

bool AudioOutputConverter::ensureBuffer(unsigned char **buffer, size_t *capacity, size_t required)
{
	if (*capacity >= required) {
		return true;
	}

	unsigned char *newBuffer = new (std::nothrow) unsigned char[required];
	if (!newBuffer) {
		meddbg("Failed to allocate conversion buffer, bytes: %u\n", required);
		return false;
	}

	delete[] *buffer;
	*buffer = newBuffer;
	*capacity = required;
	return true;
}

size_t AudioOutputConverter::getSourceFrameBytes() const
{
	return mSource.channels * (pcm_format_to_bits(mSource.format) >> 3);
}

size_t AudioOutputConverter::getOutputFrameBytes() const
{
	return mOutput.channels * (pcm_format_to_bits(mOutput.format) >> 3);
}

bool AudioOutputConverter::isConfigured() const
{
	return mConfigured;
}

size_t AudioOutputConverter::getInputBytesForOutput(size_t outputBytes) const
{
	if (!mConfigured) {
		return outputBytes;
	}

	size_t outputFrameBytes = getOutputFrameBytes();
	size_t sourceFrameBytes = getSourceFrameBytes();
	if (outputFrameBytes == 0 || sourceFrameBytes == 0) {
		return 0;
	}

	uint64_t outputFrames = outputBytes / outputFrameBytes;
	if (outputFrames > UINT64_MAX / mSource.sampleRate) {
		return 0;
	}
	uint64_t sourceFrames = outputFrames * mSource.sampleRate / mOutput.sampleRate;
	if (sourceFrames == 0 && outputFrames > 0) {
		sourceFrames = 1;
	}
	if (sourceFrames > SIZE_MAX / sourceFrameBytes) {
		return 0;
	}
	return (size_t)sourceFrames * sourceFrameBytes;
}

bool AudioOutputConverter::convertToS16(const unsigned char *input, size_t frames)
{
	if (frames > SIZE_MAX / mSource.channels) {
		return false;
	}
	size_t samples = frames * mSource.channels;
	if (samples > SIZE_MAX / sizeof(int16_t) ||
		!ensureBuffer(&mS16Buffer, &mS16BufferSize, samples * sizeof(int16_t))) {
		return false;
	}

	int16_t *output = reinterpret_cast<int16_t *>(mS16Buffer);
	switch (mSource.format) {
	case PCM_FORMAT_S8:
		for (size_t i = 0; i < samples; i++) {
			output[i] = static_cast<int16_t>(static_cast<int8_t>(input[i])) * 256;
		}
		break;
	case PCM_FORMAT_S16_LE:
		memcpy(output, input, samples * sizeof(int16_t));
		break;
	case PCM_FORMAT_S32_LE:
		for (size_t i = 0; i < samples; i++) {
			uint32_t value = static_cast<uint32_t>(input[i * 4]) |
							 (static_cast<uint32_t>(input[i * 4 + 1]) << 8) |
							 (static_cast<uint32_t>(input[i * 4 + 2]) << 16) |
							 (static_cast<uint32_t>(input[i * 4 + 3]) << 24);
			output[i] = static_cast<int16_t>(value >> 16);
		}
		break;
	default:
		return false;
	}

	return true;
}

ssize_t AudioOutputConverter::resample(const int16_t *input, size_t frames, const unsigned char **output)
{
	uint64_t estimatedFrames = ((uint64_t)frames * mOutput.sampleRate + mSource.sampleRate - 1) /
							 mSource.sampleRate;
	if (mResampler) {
		estimatedFrames += speex_resampler_get_output_latency(mResampler) + 1;
	}

	size_t outputFrameBytes = getOutputFrameBytes();
	if (estimatedFrames > SIZE_MAX / outputFrameBytes ||
		estimatedFrames > static_cast<uint64_t>(INT_MAX) / outputFrameBytes) {
		return -1;
	}
	if (!ensureBuffer(&mOutputBuffer, &mOutputBufferSize, (size_t)estimatedFrames * outputFrameBytes)) {
		return -1;
	}

	if (!mResampler) {
		size_t bytes = frames * outputFrameBytes;
		memcpy(mOutputBuffer, input, bytes);
		*output = mOutputBuffer;
		return (ssize_t)bytes;
	}

	spx_uint32_t inputFrames = static_cast<spx_uint32_t>(frames);
	spx_uint32_t outputFrames = static_cast<spx_uint32_t>(estimatedFrames);
	int ret = speex_resampler_process_interleaved_int(mResampler, input, &inputFrames,
													  reinterpret_cast<int16_t *>(mOutputBuffer), &outputFrames);
	if (ret != RESAMPLER_ERR_SUCCESS || inputFrames != frames) {
		meddbg("Output resampling failed, error: %d, consumed: %u/%u\n", ret, inputFrames, frames);
		return -1;
	}

	*output = mOutputBuffer;
	return static_cast<ssize_t>((size_t)outputFrames * outputFrameBytes);
}

ssize_t AudioOutputConverter::convert(const unsigned char *input, size_t inputBytes, const unsigned char **output)
{
	if (!mConfigured || !input || !output) {
		return -1;
	}

	size_t sourceFrameBytes = getSourceFrameBytes();
	if (sourceFrameBytes == 0 || sourceFrameBytes > sizeof(mPendingInput) ||
		inputBytes > SIZE_MAX - mPendingInputBytes) {
		return -1;
	}

	size_t totalBytes = mPendingInputBytes + inputBytes;
	if (totalBytes < sourceFrameBytes) {
		memcpy(mPendingInput + mPendingInputBytes, input, inputBytes);
		mPendingInputBytes = totalBytes;
		*output = nullptr;
		return 0;
	}

	const unsigned char *completeInput = input;
	if (mPendingInputBytes > 0) {
		if (!ensureBuffer(&mInputBuffer, &mInputBufferSize, totalBytes)) {
			return -1;
		}
		memcpy(mInputBuffer, mPendingInput, mPendingInputBytes);
		memcpy(mInputBuffer + mPendingInputBytes, input, inputBytes);
		completeInput = mInputBuffer;
	}

	size_t completeBytes = totalBytes - totalBytes % sourceFrameBytes;
	size_t trailingBytes = totalBytes - completeBytes;
	if (trailingBytes > 0) {
		memcpy(mPendingInput, completeInput + completeBytes, trailingBytes);
	}
	mPendingInputBytes = trailingBytes;

	size_t frames = completeBytes / sourceFrameBytes;
	if (frames > INT_MAX || !convertToS16(completeInput, frames)) {
		return -1;
	}

	const int16_t *resampleInput = reinterpret_cast<const int16_t *>(mS16Buffer);
	if (mSource.channels != mOutput.channels) {
		if (frames > SIZE_MAX / mOutput.channels / sizeof(int16_t)) {
			return -1;
		}
		size_t rechannelBytes = frames * mOutput.channels * sizeof(int16_t);
		if (!ensureBuffer(&mRechannelBuffer, &mRechannelBufferSize, rechannelBytes)) {
			return -1;
		}
		int32_t convertedFrames = rechannel(ch2layout(mSource.channels), ch2layout(mOutput.channels),
										   resampleInput, frames,
										   reinterpret_cast<int16_t *>(mRechannelBuffer), frames);
		if (convertedFrames != static_cast<int32_t>(frames)) {
			meddbg("Failed to rechannel source PCM: %d/%u\n", convertedFrames, frames);
			return -1;
		}
		resampleInput = reinterpret_cast<const int16_t *>(mRechannelBuffer);
	}

	return resample(resampleInput, frames, output);
}

ssize_t AudioOutputConverter::drain(const unsigned char **output)
{
	if (!mConfigured || !output) {
		return -1;
	}
	if (mPendingInputBytes > 0) {
		meddbg("Discarding %u incomplete source PCM byte(s) at EOS\n", (unsigned int)mPendingInputBytes);
		mPendingInputBytes = 0;
	}
	if (!mResampler) {
		*output = nullptr;
		return 0;
	}

	spx_uint32_t inputFrames = speex_resampler_get_input_latency(mResampler);
	if (inputFrames == 0) {
		*output = nullptr;
		return 0;
	}

	if (inputFrames > SIZE_MAX / mOutput.channels / sizeof(int16_t)) {
		return -1;
	}
	size_t inputBytes = (size_t)inputFrames * mOutput.channels * sizeof(int16_t);
	if (!ensureBuffer(&mRechannelBuffer, &mRechannelBufferSize, inputBytes)) {
		return -1;
	}
	memset(mRechannelBuffer, 0xff, inputBytes);
	return resample(reinterpret_cast<const int16_t *>(mRechannelBuffer), inputFrames, output);
}

} // namespace stream
} // namespace media
