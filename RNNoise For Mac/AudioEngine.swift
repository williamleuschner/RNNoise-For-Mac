//
//  AudioEngine.swift
//  RNNoise For Mac
//
//  Created by William on 30 Dec.
//

import AVFoundation

enum RNNoiseError: Error {
    case missingAU
}

class AudioEngine {

    private let engine = AVAudioEngine()

    private var denoiseAU: AVAudioUnit? = nil
    private let inputNode: AVAudioNode
    private let outputNode: AVAudioNode

    public init() {
        inputNode = engine.inputNode
        outputNode = engine.outputNode
        engine.connect(inputNode, to: engine.mainMixerNode, format: inputNode.inputFormat(forBus: 0))
        // TODO: Find a way to alert when the mic sample rate and output sample rate aren't 48 kHz
    }

    func setupDenoiseAU(completion: @escaping (Result<Bool, Error>) -> Void) {
        DispatchQueue.global(qos: .default).async {
            let description = AudioComponentDescription(componentType: 0x61756678, // "aufx"
                                                        componentSubType: 0x726e6e7a, // "rnnz"
                                                        componentManufacturer: 0x58495048, // "XIPH"
                                                        componentFlags: 0,
                                                        componentFlagsMask: 0)
            let components = AVAudioUnitComponentManager.shared().components(matching: description)
            if components.count != 1 {
                // This shouldn't ever happen because the AU is part of this
                // application, but just in case.
                DispatchQueue.main.async {
                    completion(.failure(RNNoiseError.missingAU))
                }
                return
            }
            let uniqueDescription = components[0].audioComponentDescription
            let options = AudioComponentInstantiationOptions.loadOutOfProcess
            AVAudioUnit.instantiate(with: uniqueDescription, options: options) { avAudioUnit, error in
                guard error == nil else {
                    DispatchQueue.main.async {
                        completion(.failure(error!))
                    }
                    return
                }
                self.connect(avAudioUnit: avAudioUnit) {
                    DispatchQueue.main.async {
                        completion(.success(true))
                    }
                }
            }
        }
    }

    func connect(avAudioUnit: AVAudioUnit?, completion: @escaping (() -> Void) = {}) {
        // Disconnect the current audio unit, if present.
        if let denoiseAU = self.denoiseAU {
            engine.disconnectNodeInput(denoiseAU)
            engine.detach(denoiseAU)
        }

        // Connect the current audio unit.
        if let newAU = avAudioUnit {
            // TODO: if there is no new AU, this will just disconnect and leave it? Should that be how it works?
            let format = inputNode.outputFormat(forBus: 0)
            self.denoiseAU = newAU
            engine.attach(newAU)
            engine.connect(inputNode, to: newAU, format: format)
            engine.connect(newAU, to: engine.mainMixerNode, format: format)
        }

        completion()
    }
}
