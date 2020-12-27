//
//  RNNoise__macOS_DSPKernel.hpp
//  RNNoise (macOS)
//
//  Created by William on 24 Dec.
//

#ifndef RNNoise__macOS_DSPKernel_hpp
#define RNNoise__macOS_DSPKernel_hpp

#include "DSPKernel.hpp"
#include "rnnoise.h"
#include <vector>
#include <limits>

enum {
    voiceConfidenceThreshold = 0,
    noiseReductionVolume = 1,
};

/*
 RNNoise__macOS_DSPKernel
 Performs simple copying of the input signal to the output.
 As a non-ObjC class, this is safe to use from render thread.
 */
class RNNoise__macOS_DSPKernel : public DSPKernel {
public:
    
    // MARK: Member Functions

    RNNoise__macOS_DSPKernel() {}

    ~RNNoise__macOS_DSPKernel() {
        for (int chanID = 0; chanID < chanCount; ++chanID) {
            rnnoise_destroy(denoiseStates[chanID]);
        }
    }

    void init(int channelCount, double inSampleRate) {
        chanCount = channelCount;
        sampleRate = float(inSampleRate);
        denoiseStates = std::vector<DenoiseState*>(channelCount);
        for (int chanID = 0; chanID < chanCount; ++chanID) {
            denoiseStates[chanID] = rnnoise_create(NULL);
        }
    }

    void reset() {
        for (int chanID = 0; chanID < chanCount; ++chanID) {
            rnnoise_destroy(denoiseStates[chanID]);
            denoiseStates[chanID] = rnnoise_create(NULL);
        }
    }

    bool isBypassed() {
        return bypassed;
    }

    void setBypass(bool shouldBypass) {
        bypassed = shouldBypass;
    }

    void setParameter(AUParameterAddress address, AUValue value) {
        switch (address) {
            case voiceConfidenceThreshold:

                break;
            case noiseReductionVolume:

                break;
        }
    }

    AUValue getParameter(AUParameterAddress address) {
        switch (address) {
            case voiceConfidenceThreshold:
                // Return the goal. It is not thread safe to return the ramping value.
                return 0.f;

            case noiseReductionVolume:
                return 0.f;

            default: return 0.f;
        }
    }

    void setBuffers(AudioBufferList* inBufferList, AudioBufferList* outBufferList) {
        inBufferListPtr = inBufferList;
        outBufferListPtr = outBufferList;
    }

    void process(AUAudioFrameCount frameCount, AUAudioFrameCount bufferOffset) override {
        if (bypassed) {
            // Pass the samples through
            for (int channel = 0; channel < chanCount; ++channel) {
                if (inBufferListPtr->mBuffers[channel].mData ==  outBufferListPtr->mBuffers[channel].mData) {
                    continue;
                }
                
                for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
                    const int frameOffset = int(frameIndex + bufferOffset);
                    const float* in  = (float*)inBufferListPtr->mBuffers[channel].mData  + frameOffset;
                    float* out = (float*)outBufferListPtr->mBuffers[channel].mData + frameOffset;
                    *out = *in;
                }
            }
            return;
        }
        
        // Perform per sample dsp on the incoming float *in before assigning it to *out
        for (int channel = 0; channel < chanCount; ++channel) {
        
            // Get pointer to immutable input buffer and mutable output buffer
            const float* in = (float*)inBufferListPtr->mBuffers[channel].mData;
            float* out = (float*)outBufferListPtr->mBuffers[channel].mData;

            // The library expects floating point values in [SHORT_MIN, SHORT_MAX],
            // because it is extremely academic. Convert by multiplying.
            float scaled[frameCount];
            for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
                const int frameOffset = int(frameIndex + bufferOffset);
                scaled[frameOffset] = in[frameOffset] * std::numeric_limits<short>::max();
            }

            float denoised[frameCount];
            if (frameCount == rnnoiseFramesPerBuffer) {
                // Happy path: same number of samples as expected, no chunking/padding necessary.
                rnnoise_process_frame(denoiseStates[channel], (scaled + bufferOffset), (denoised + bufferOffset));
            } else {
                // Unhappy path: must chunk and/or zero-pad.
                float *noisyChunkStart = scaled + bufferOffset;
                float *denoisedChunkStart = denoised + bufferOffset;
                AUAudioFrameCount remainingFrames = frameCount;
                // Take whole chunks with no padding until there is less than
                // one whole chunk of frames remaining to process.
                while (remainingFrames >= rnnoiseFramesPerBuffer) {
                    rnnoise_process_frame(denoiseStates[channel], noisyChunkStart, denoisedChunkStart);
                    remainingFrames -= rnnoiseFramesPerBuffer;
                    noisyChunkStart += rnnoiseFramesPerBuffer;
                    denoisedChunkStart += rnnoiseFramesPerBuffer;
                }
                // Copy the remaining frames into a temporary buffer, padded
                // with zeroes, and process that buffer.
                if (remainingFrames != 0) {
                    float lastNoisyChunk[rnnoiseFramesPerBuffer];
                    float lastDenoisedChunk[rnnoiseFramesPerBuffer];
                    for (int idx = 0; idx < rnnoiseFramesPerBuffer; ++idx) {
                        if (idx < remainingFrames) {
                            lastNoisyChunk[idx] = noisyChunkStart[idx];
                        } else {
                            lastNoisyChunk[idx] = 0.0f;
                        }
                    }
                    rnnoise_process_frame(denoiseStates[channel], lastNoisyChunk, lastDenoisedChunk);
                    for (int idx = 0; idx < remainingFrames; ++idx) {
                        denoisedChunkStart[idx] = lastDenoisedChunk[idx];
                    }
                }
            }

            // Convert the data back to [-1, 1], for the rest of the AU graph.
            for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
                const int frameOffset = int(frameIndex + bufferOffset);
                out[frameOffset] = denoised[frameOffset] / std::numeric_limits<short>::max();
            }
        }
    }

    // MARK: Member Variables

private:
    int chanCount = 0;
    float sampleRate = 48000.0;
    bool bypassed = false;
    std::vector<DenoiseState*> denoiseStates;
    // The library's RNN was trained on 480-sample buffers. All supplied buffers must have 480 samples.
    const int rnnoiseFramesPerBuffer = 480;
    AudioBufferList* inBufferListPtr = nullptr;
    AudioBufferList* outBufferListPtr = nullptr;
};

#endif /* RNNoise__macOS_DSPKernel_hpp */
