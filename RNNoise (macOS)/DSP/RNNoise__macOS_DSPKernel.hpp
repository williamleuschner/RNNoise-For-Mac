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
            free(denoiseBuffers[chanID]);
        }
    }

    void init(int channelCount, double inSampleRate) {
        chanCount = channelCount;
        sampleRate = float(inSampleRate);
        // Introduce enough latency so a minimum of a single sample will generate output
        bufferedInputSamples = 479;
        bufferedOutputSamples = 0;
        denoiseStates = std::vector<DenoiseState*>(channelCount);
        denoiseBuffers = std::vector<rnBuffer*>(channelCount);
        for (int chanID = 0; chanID < chanCount; ++chanID) {
            denoiseStates[chanID] = rnnoise_create(NULL);
            denoiseBuffers[chanID] = (rnBuffer*) calloc(1, sizeof(rnBuffer));
        }
    }

    void reset() {
        bufferedInputSamples = 479;
        bufferedOutputSamples = 0;
        for (int chanID = 0; chanID < chanCount; ++chanID) {
            rnnoise_destroy(denoiseStates[chanID]);
            denoiseStates[chanID] = rnnoise_create(NULL);
            memset(denoiseBuffers[chanID], 0, sizeof(rnBuffer));
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
    
    double getLatency() {
        return double(rnnoiseFramesPerBuffer - 1) / sampleRate;
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
        
        
        // These are constant for all channels, so cache in at start
        const int inBufferedInputSamples = bufferedInputSamples;
        int outBufferedInputSamples = 0;
        const int inBufferedOutputSamples = bufferedOutputSamples;
        int outBufferedOutputSamples = 0;
        
        // Perform per sample dsp on the incoming float *in before assigning it to *out
        for (int channel = 0; channel < chanCount; ++channel) {
        
            // Get pointer to immutable input buffer and mutable output buffer
            const float* in = (float*)inBufferListPtr->mBuffers[channel].mData;
            float* out = (float*)outBufferListPtr->mBuffers[channel].mData;
            
            float* inBuffer = &denoiseBuffers[channel]->in[0];
            float* outBuffer = &denoiseBuffers[channel]->out[0];

            // The library expects floating point values in [SHORT_MIN, SHORT_MAX],
            // because it is extremely academic. Convert by multiplying.
            float scaled[frameCount + inBufferedInputSamples];
            // buffered input from previous calls is pre-scaled
            memcpy(scaled, inBuffer, sizeof(float) * inBufferedInputSamples);
            // new input needs to be scaled
            const float mulscale = float(std::numeric_limits<short>::max());
            for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
                const int frameOffset = int(frameIndex + bufferOffset);
                const int bufferOffset = int(frameIndex + inBufferedInputSamples);
                scaled[bufferOffset] = in[frameOffset] * mulscale;
            }
            
            // We always want to process ahead of input, so produce at least a block
            const int blockCount = int((frameCount + inBufferedInputSamples) / rnnoiseFramesPerBuffer);

            float denoised[inBufferedOutputSamples + blockCount * rnnoiseFramesPerBuffer];
            memcpy(denoised, outBuffer, sizeof(float) * inBufferedOutputSamples);
            {
                // Unhappy path: must chunk and/or zero-pad.
                float *noisyChunkStart = scaled;
                float *denoisedChunkStart = denoised + inBufferedOutputSamples;
                AUAudioFrameCount remainingFrames = frameCount + inBufferedInputSamples;
                // Take whole chunks with no padding until there is less than
                // one whole chunk of frames remaining to process.
                while (remainingFrames >= rnnoiseFramesPerBuffer) {
                    rnnoise_process_frame(denoiseStates[channel], denoisedChunkStart, noisyChunkStart);
//                    memcpy(denoisedChunkStart, noisyChunkStart, rnnoiseFramesPerBuffer * sizeof(float));
                    remainingFrames -= rnnoiseFramesPerBuffer;
                    noisyChunkStart += rnnoiseFramesPerBuffer;
                    denoisedChunkStart += rnnoiseFramesPerBuffer;
                }
                // Copy the remaining frames into a temporary buffer, padded
                // with zeroes, and process that buffer.
                if (remainingFrames != 0) {
                    memcpy(inBuffer, noisyChunkStart, sizeof(float) * remainingFrames);
                    // It's okay for every channel to clobber this, they should be in sync anyway
                    outBufferedInputSamples = remainingFrames;
                }
            }

            // Convert the data back to [-1, 1], for the rest of the AU graph.
            const float divscale = 1.0f / float(std::numeric_limits<short>::max());
            for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
                const int frameOffset = int(frameIndex + bufferOffset);
                out[frameOffset] = denoised[frameIndex] * divscale;
            }
            
            const int blockFrameCount = int(blockCount * rnnoiseFramesPerBuffer + inBufferedOutputSamples);
            
            // And here's the overbuffer of output
            if (frameCount < blockFrameCount) {
                const int framesRemaining = int(blockFrameCount - frameCount);
                memcpy(outBuffer, denoised + frameCount, sizeof(float) * framesRemaining);
                // Okay to clobber this too, all channels should produce equal counts
                outBufferedOutputSamples = framesRemaining;
            }
        }
        
        // Now stash these for the next call
        bufferedInputSamples = outBufferedInputSamples;
        bufferedOutputSamples = outBufferedOutputSamples;
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
    int bufferedInputSamples = 479;
    int bufferedOutputSamples = 0;
    // We will never buffer more than a single block worth of audio, in or out. The combined buffers should total to 479 samples.
    typedef struct rnBuffer { float in[480]; float out[480]; } rnBuffer;
    std::vector<rnBuffer*> denoiseBuffers;
};

#endif /* RNNoise__macOS_DSPKernel_hpp */
