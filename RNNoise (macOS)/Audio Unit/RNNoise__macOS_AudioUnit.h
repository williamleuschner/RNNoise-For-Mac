//
//  RNNoise__macOS_AudioUnit.h
//  RNNoise (macOS)
//
//  Created by William on 24 Dec.
//

#import <AudioToolbox/AudioToolbox.h>
#import "RNNoise__macOS_DSPKernelAdapter.h"

// Define parameter addresses.
extern const AudioUnitParameterID voiceConfidenceThresholdID;
extern const AudioUnitParameterID noiseReductionVolumeID;

@interface RNNoise__macOS_AudioUnit : AUAudioUnit

@property (nonatomic, readonly) RNNoise__macOS_DSPKernelAdapter *kernelAdapter;
- (void)setupAudioBuses;
- (void)setupParameterTree;
- (void)setupParameterCallbacks;
@end
