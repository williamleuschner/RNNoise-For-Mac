//
//  RNNoise__macOS_DSPKernel.hpp
//  RNNoise (macOS)
//
//  Created by William on 24 Dec.
//

#ifndef RNNoise__macOS_DSPKernel_hpp
#define RNNoise__macOS_DSPKernel_hpp

#include "DSPKernel.hpp"
extern "C" {
#include "rnnoise.h"
}
#include <vector>
#include <limits>
#include <thread>
#include <mutex>

enum {
    speechConfidenceThresholdPct = 0,
    voxReleaseDelay = 1,
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
        // Block until the DSP code finishes.
        doingDSP.lock();
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
        voxReleaseCounterByChannel = std::vector<int>(channelCount);
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
            voxReleaseCounterByChannel[chanID] = 0;
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
            case speechConfidenceThresholdPct:
                speechConfidenceThreshold = value;
                break;
            case voxReleaseDelay:
                voxReleaseBufferCount = int(value);
                break;
        }
    }

    AUValue getParameter(AUParameterAddress address) {
        switch (address) {
            case speechConfidenceThresholdPct:
                // Return the goal. It is not thread safe to return the ramping value.
                return speechConfidenceThreshold;

            case voxReleaseDelay:
                return float(voxReleaseBufferCount);

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
        // Acquire a lock on the doingDSP mutex until this variable goes out of
        // scope, to prevent the destructor from destructing during DSP.
        std::lock_guard<std::mutex> guard(doingDSP);
        if (bypassed) {
            // Don't do any DSP on the signal when this AU is in bypass mode.
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
        
        
        // These are constant for all channels, so compute before looping on channels.
        const int inBufferedInputSamples = bufferedInputSamples;
        int outBufferedInputSamples = 0;
        const int inBufferedOutputSamples = bufferedOutputSamples;
        int outBufferedOutputSamples = 0;

        for (int channel = 0; channel < chanCount; ++channel) {
        
            // Get pointer to immutable input buffer and mutable output buffer.
            const float* in = (float*)inBufferListPtr->mBuffers[channel].mData;
            float* out = (float*)outBufferListPtr->mBuffers[channel].mData;

            // Get pointers to per-channel, internal input and output buffers.
            float* inBuffer = &denoiseBuffers[channel]->in[0];
            float* outBuffer = &denoiseBuffers[channel]->out[0];

            // The library expects floating point values in [SHORT_MIN, SHORT_MAX],
            // because it is extremely academic.

            // Create a temporary buffer to store scaled samples.
            float scaled[frameCount + inBufferedInputSamples];
            // Previous calls have already scaled the samples in the internal input buffer.
            memcpy(scaled, inBuffer, sizeof(float) * inBufferedInputSamples);
            // Scale the samples that this AU just received from the previous node in the graph.
            const float mulscale = float(std::numeric_limits<short>::max());
            for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
                // Offset into the buffer provided by the ancestor node.
                const int frameOffset = int(frameIndex + bufferOffset);
                // Offset into the temporary scaling buffer.
                const int bufferOffset = int(frameIndex + inBufferedInputSamples);
                scaled[bufferOffset] = in[frameOffset] * mulscale;
            }
            
            // We always want to process ahead of input, so produce at least a block
            const int blockCount = int((frameCount + inBufferedInputSamples) / rnnoiseFramesPerBuffer);

            // Create a temporary buffer to store samples that have been denoised.
            float denoised[inBufferedOutputSamples + blockCount * rnnoiseFramesPerBuffer];
            // Copy any samples that previous calls have already denoised from
            // the internal output buffer into the temporary one.
            memcpy(denoised, outBuffer, sizeof(float) * inBufferedOutputSamples);

            // Get a pointer to the start of the current 480-sample chunk of noisy samples.
            float *currentNoisyChunk = scaled;
            // Get a pointer to the start of the current 480-sample chunk of denoised samples.
            float *currentDenoisedChunk = denoised + inBufferedOutputSamples;
            // Calculate how many frames/samples of audio data the AU has to
            // work with.
            AUAudioFrameCount remainingFrames = frameCount + inBufferedInputSamples;
            // Grab 480-sample chunks from the buffer pointers above, denoise
            // them, and then update the remaining sample/frame count and the
            // buffer pointers.
            while (remainingFrames >= rnnoiseFramesPerBuffer) {
                float frameSpeechConfidence = rnnoise_process_frame(denoiseStates[channel],
                                                                    currentDenoisedChunk,
                                                                    currentNoisyChunk);
                // Noise gate: if this frame is probably speech, set the release
                // counter to the release buffer count.
                if (frameSpeechConfidence >= speechConfidenceThreshold) {
                    voxReleaseCounterByChannel[channel] = voxReleaseBufferCount;
                }
                // Noise gate: if the release counter for this channel is at
                // zero, overwrite the denoised chunk with zeroes.
                if (voxReleaseCounterByChannel[channel] <= 0) {
                    memset(currentDenoisedChunk, 0, rnnoiseFramesPerBuffer);
                } else {
                    // (Otherwise, decrement the release counter.)
                    voxReleaseCounterByChannel[channel] -= 1;
                }
                remainingFrames -= rnnoiseFramesPerBuffer;
                currentNoisyChunk += rnnoiseFramesPerBuffer;
                currentDenoisedChunk += rnnoiseFramesPerBuffer;
            }
            // Copy leftover frames that didn't fit exactly into a 480-sample
            // chunk from the end of the noisy buffer to the input buffer.
            if (remainingFrames != 0) {
                memcpy(inBuffer, currentNoisyChunk, sizeof(float) * remainingFrames);
                // It's okay for every channel to clobber this, they should be in sync anyway
                outBufferedInputSamples = remainingFrames;
            }

            // Convert the data back to [-1, 1], for the rest of the AU graph.
            // The division is done once because floating point division costs
            // more cycles than floating point multiplication.
            const float divscale = 1.0f / float(std::numeric_limits<short>::max());
            for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
                const int frameOffset = int(frameIndex + bufferOffset);
                out[frameOffset] = denoised[frameIndex] * divscale;
            }

            // Calculate the number of frames/samples that were just processed.
            const int blockFrameCount = int(blockCount * rnnoiseFramesPerBuffer + inBufferedOutputSamples);
            
            // If the number of samples/frames this AU must output is smaller
            // than the number of samples/frames that were just denoised, move
            // the extras into an internal output buffer for the next call.
            if (frameCount < blockFrameCount) {
                const int framesRemaining = int(blockFrameCount - frameCount);
                memcpy(outBuffer, denoised + frameCount, sizeof(float) * framesRemaining);
                // Okay to clobber this too, all channels should produce equal counts
                outBufferedOutputSamples = framesRemaining;
            }
        }
        
        // Now stash these for the next call.
        bufferedInputSamples = outBufferedInputSamples;
        bufferedOutputSamples = outBufferedOutputSamples;
    }

    // MARK: Member Variables

private:
    int chanCount = 0;
    float sampleRate = 48000.0;
    bool bypassed = false;
    std::vector<DenoiseState*> denoiseStates;
    // The library's RNN was trained on 480-sample buffers. All supplied buffers
    // must have 480 samples.
    const int rnnoiseFramesPerBuffer = 480;
    AudioBufferList* inBufferListPtr = nullptr;
    AudioBufferList* outBufferListPtr = nullptr;
    int bufferedInputSamples = 479;
    int bufferedOutputSamples = 0;
    // We will never buffer more than a single block worth of audio, in or out.
    // The combined buffers should total to 479 samples.
    typedef struct rnBuffer { float in[480]; float out[480]; } rnBuffer;
    std::vector<rnBuffer*> denoiseBuffers;
    // RNNoise reports a confidence level that the buffer it just processed
    // contained speech data. This is the threshold for that confidence level
    // used to trigger a noise gate.
    float speechConfidenceThreshold = 0.95;
    // When the speech confidence from RNNoise drops below
    // speechConfidenceThreshold, wait this many buffers before zeroing out the
    // signal (aka activating the noise gate).
    int voxReleaseBufferCount = 10;
    // Per-channel counters to keep track of how many buffer lengths remain
    // before the noise gate activates. 0 means the gate is closed.
    std::vector<int> voxReleaseCounterByChannel;
    // Avoid deallocating resources while the DSP code is running.
    std::mutex doingDSP;
};

#endif /* RNNoise__macOS_DSPKernel_hpp */
