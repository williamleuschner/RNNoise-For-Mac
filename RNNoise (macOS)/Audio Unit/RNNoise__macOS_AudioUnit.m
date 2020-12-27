//
//  RNNoise__macOS_AudioUnit.m
//  RNNoise (macOS)
//
//  Created by William on 24 Dec.
//

#import "RNNoise__macOS_AudioUnit.h"

#import <AVFoundation/AVFoundation.h>

// Define parameter addresses.
const AudioUnitParameterID voiceConfidenceThresholdID = 0;
const AudioUnitParameterID noiseReductionVolumeID = 1;

@interface RNNoise__macOS_AudioUnit ()

@property (nonatomic, readwrite) AUParameterTree *parameterTree;
@property AUAudioUnitBusArray *inputBusArray;
@property AUAudioUnitBusArray *outputBusArray;
@end


@implementation RNNoise__macOS_AudioUnit
@synthesize parameterTree = _parameterTree;

- (instancetype)initWithComponentDescription:(AudioComponentDescription)componentDescription options:(AudioComponentInstantiationOptions)options error:(NSError **)outError {
    self = [super initWithComponentDescription:componentDescription options:options error:outError];
    
    if (self == nil) { return nil; }

	_kernelAdapter = [[RNNoise__macOS_DSPKernelAdapter alloc] init];

	self.maximumFramesToRender = _kernelAdapter.maximumFramesToRender;

	[self setupAudioBuses];
	[self setupParameterTree];
	[self setupParameterCallbacks];
    return self;
}

#pragma mark - AUAudioUnit Setup

- (void)setupAudioBuses {
	// Create the input and output bus arrays.
	_inputBusArray  = [[AUAudioUnitBusArray alloc] initWithAudioUnit:self
															 busType:AUAudioUnitBusTypeInput
															  busses: @[_kernelAdapter.inputBus]];
	_outputBusArray = [[AUAudioUnitBusArray alloc] initWithAudioUnit:self
															 busType:AUAudioUnitBusTypeOutput
															  busses: @[_kernelAdapter.outputBus]];
}

- (void)setupParameterTree {
    // Create parameter objects.
    AUParameter *voiceConfidenceThreshold = [AUParameterTree createParameterWithIdentifier:@"voiceConfidenceThreshold"
																	name:@"Voice Confidence Threshold"
																 address:voiceConfidenceThresholdID
																	 min:0
																	 max:100
																	unit:kAudioUnitParameterUnit_Percent
																unitName:nil
																   flags:0
															valueStrings:nil
													 dependentParameters:nil];
    AUParameter *noiseReductionVolume = [AUParameterTree createParameterWithIdentifier:@"noiseReductionVolume"
                                                                                  name:@"Noise Reduction Volume"
                                                                               address:noiseReductionVolumeID
                                                                                   min:0
                                                                                   max:100
                                                                                  unit:kAudioUnitParameterUnit_Percent
                                                                              unitName:nil
                                                                                 flags:0
                                                                          valueStrings:nil
                                                                   dependentParameters:nil];

    // Initialize the parameter values.
    voiceConfidenceThreshold.value = 0.95;
    noiseReductionVolume.value = 0.95;

    // Create the parameter tree.
    _parameterTree = [AUParameterTree createTreeWithChildren:@[ voiceConfidenceThreshold, noiseReductionVolume ]];
}

- (void)setupParameterCallbacks {
	// Make a local pointer to the kernel to avoid capturing self.
	__block RNNoise__macOS_DSPKernelAdapter * kernelAdapter = _kernelAdapter;

	// implementorValueObserver is called when a parameter changes value.
	_parameterTree.implementorValueObserver = ^(AUParameter *param, AUValue value) {
		[kernelAdapter setParameter:param value:value];
	};

	// implementorValueProvider is called when the value needs to be refreshed.
	_parameterTree.implementorValueProvider = ^(AUParameter *param) {
		return [kernelAdapter valueForParameter:param];
	};

	// A function to provide string representations of parameter values.
	_parameterTree.implementorStringFromValueCallback = ^(AUParameter *param, const AUValue *__nullable valuePtr) {
		AUValue value = valuePtr == nil ? param.value : *valuePtr;

		return [NSString stringWithFormat:@"%.f", value];
	};
}

#pragma mark - AUAudioUnit Overrides

// If an audio unit has input, an audio unit's audio input connection points.
// Subclassers must override this property getter and should return the same object every time.
// See sample code.
- (AUAudioUnitBusArray *)inputBusses {
	return _inputBusArray;
}

// An audio unit's audio output connection points.
// Subclassers must override this property getter and should return the same object every time.
// See sample code.
- (AUAudioUnitBusArray *)outputBusses {
	return _outputBusArray;
}

// Allocate resources required to render.
// Subclassers should call the superclass implementation.
- (BOOL)allocateRenderResourcesAndReturnError:(NSError **)outError {
	if (_kernelAdapter.outputBus.format.channelCount != _kernelAdapter.inputBus.format.channelCount) {
		if (outError) {
			*outError = [NSError errorWithDomain:NSOSStatusErrorDomain code:kAudioUnitErr_FailedInitialization userInfo:nil];
		}
		// Notify superclass that initialization was not successful
		self.renderResourcesAllocated = NO;

		return NO;
	}

	[super allocateRenderResourcesAndReturnError:outError];
	[_kernelAdapter allocateRenderResources];
	return YES;
}

// Deallocate resources allocated in allocateRenderResourcesAndReturnError:
// Subclassers should call the superclass implementation.
- (void)deallocateRenderResources {
	[_kernelAdapter deallocateRenderResources];

    // Deallocate your resources.
    [super deallocateRenderResources];
}

- (NSTimeInterval) latency {
    // This AU does no buffering (so far) and thus I'm going to pretend it has
    // no latency for the sake of simplicity.
    return 0;
}

- (NSTimeInterval) tailTime {
    // Tail time may actually be negative because this AU removes noise, but
    // again 0 for the sake of simplicity.
    return 0;
}

#pragma mark - AUAudioUnit (AUAudioUnitImplementation)

// Block which subclassers must provide to implement rendering.
- (AUInternalRenderBlock)internalRenderBlock {
	return _kernelAdapter.internalRenderBlock;
}

@end

