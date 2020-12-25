//
//  RNNoise__macOS_DSPKernel.hpp
//  RNNoise (macOS)
//
//  Created by William on 24 Dec.
//

#ifndef RNNoise__macOS_DSPKernel_hpp
#define RNNoise__macOS_DSPKernel_hpp

#import "DSPKernel.hpp"
#import "rnnoise.h"
#import <vector>

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

#if 0
            if (frameCount == rnnoiseSamplesPerBuffer) {
                // Happy path: same number of samples as expected, no chunking/padding necessary
            } else {
                // Unhappy path: must chunk and/or pad
            }
#endif
            for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
                const int frameOffset = int(frameIndex + bufferOffset);
                
                // Do your sample by sample dsp here...
                // Make it half as loud for now so I can verify that I'm doing the rest of this correctly.
                out[frameOffset] = 0.5 * in[frameOffset];
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
    const int rnnoiseSamplesPerBuffer = 480;
    AudioBufferList* inBufferListPtr = nullptr;
    AudioBufferList* outBufferListPtr = nullptr;
};

#endif /* RNNoise__macOS_DSPKernel_hpp */
