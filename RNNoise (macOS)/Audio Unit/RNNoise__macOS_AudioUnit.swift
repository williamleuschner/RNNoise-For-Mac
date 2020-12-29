//
//  RNNoise__macOS_AudioUnit.swift
//  RNNoise (macOS)
//
//  Created by William on 29 Dec.
//

import Foundation
import AudioToolbox
import AVFoundation
import CoreAudioKit

public class RNNoise__macOS_AudioUnit: AUAudioUnit {

    private let kernelAdapter: RNNoise__macOS_DSPKernelAdapter

    lazy private var inputBusArray: AUAudioUnitBusArray = {
        AUAudioUnitBusArray(audioUnit: self, busType: .input, busses: [kernelAdapter.inputBus])
    }()

    lazy private var outputBusArray: AUAudioUnitBusArray = {
        AUAudioUnitBusArray(audioUnit: self, busType: .output, busses: [kernelAdapter.outputBus])
    }()

    public override var inputBusses: AUAudioUnitBusArray {
        return inputBusArray
    }

    public override var outputBusses: AUAudioUnitBusArray {
        return outputBusArray
    }

    public override var parameterTree: AUParameterTree? {
        get { return AUParameterTree.createTree(withChildren: []) }
        set { /* no setting parameter tree */ }
    }

    private var _currentPreset: AUAudioUnitPreset?

    /// The currently selected preset.
    public override var currentPreset: AUAudioUnitPreset? {
        get { return _currentPreset }
        set {
            // If the newValue is nil, return.
            guard newValue != nil else {
                _currentPreset = nil
                return
            }

            _currentPreset = nil

//            // Factory presets need to always have a number >= 0.
//            if preset.number >= 0 {
//                let values = factoryPresetValues[preset.number]
//                parameters.setParameterValues(cutoff: values.cutoff, resonance: values.resonance)
//                _currentPreset = preset
//            }
//            // User presets are always negative.
//            else {
//                // Attempt to restore the archived state for this user preset.
//                do {
//                    fullStateForDocument = try presetState(for: preset)
//                    // Set the currentPreset after we've successfully restored the state.
//                    _currentPreset = preset
//                } catch {
//                    print("Unable to restore set for preset \(preset.name)")
//                }
//            }
        }
    }

    public override var supportsUserPresets: Bool {
        return false
    }

    public override init(componentDescription: AudioComponentDescription, options: AudioComponentInstantiationOptions = []) throws {
        // Create adapter to communicate with underlying C++ DSP code.
        kernelAdapter = RNNoise__macOS_DSPKernelAdapter()

        try super.init(componentDescription: componentDescription, options: options)

        // Log component description values
        log(componentDescription)
    }

    private func log(_ acd: AudioComponentDescription) {

        let info = ProcessInfo.processInfo
        print("\nProcess Name: \(info.processName) PID: \(info.processIdentifier)\n")

        let message = """
        RNNoise__macOS_ (
                  type: \(acd.componentType.stringValue)
               subtype: \(acd.componentSubType.stringValue)
          manufacturer: \(acd.componentManufacturer.stringValue)
                 flags: \(String(format: "%#010x", acd.componentFlags))
        )
        """
        print(message)
    }

    public override var maximumFramesToRender: AUAudioFrameCount {
        get {
            return kernelAdapter.maximumFramesToRender
        }
        set {
            if !renderResourcesAllocated {
                kernelAdapter.maximumFramesToRender = newValue
            }
        }
    }

    public override func allocateRenderResources() throws {
        if kernelAdapter.outputBus.format.channelCount != kernelAdapter.inputBus.format.channelCount {
            throw NSError(domain: NSOSStatusErrorDomain, code: Int(kAudioUnitErr_FailedInitialization), userInfo: nil)
        }
        try super.allocateRenderResources()
        kernelAdapter.allocateRenderResources()
    }

    public override func deallocateRenderResources() {
        super.deallocateRenderResources()
        kernelAdapter.deallocateRenderResources()
    }

    public override var internalRenderBlock: AUInternalRenderBlock {
        return kernelAdapter.internalRenderBlock()
    }

    public override var canProcessInPlace: Bool {
        return true
    }

    public override var latency: TimeInterval {
        // This AU does no buffering yet, thus it has no latency to report.
        return 0;
    }

    public override var tailTime: TimeInterval {
        // This AU does not do reverb-style effects, thus it has no tail time to report.
        return 0;
    }
}

extension FourCharCode {
    var stringValue: String {
        let value = CFSwapInt32BigToHost(self)
        let bytes = [0, 8, 16, 24].map { UInt8(value >> $0 & 0x000000FF) }
        guard let result = String(bytes: bytes, encoding: .utf8) else {
            return "fail"
        }
        return result
    }
}
