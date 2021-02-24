//
//  RNNoise__macOS_AudioUnitParams.swift
//  RNNoise (macOS)
//
//  Created by William on 13 Jan.
//

import Foundation

class RNNoise__macOS_AudioUnitParams {

    private enum RNNoise__macOS_Param: AUParameterAddress {
        case speechConfidenceThresholdPct
        case gateAttackDelay
    }

    /// Parameter to control the confidence threshold below which the noise gate will activate.
    var speechConfidenceThresholdPctParam: AUParameter = {
        let parameter =
            AUParameterTree.createParameter(withIdentifier: "speechConfidenceThresholdPct",
                                            name: "Speech Confidence Threshold",
                                            address: RNNoise__macOS_Param.speechConfidenceThresholdPct.rawValue,
                                            min: 0.0,
                                            max: 1.0,
                                            unit: .percent,
                                            unitName: nil,
                                            flags: [.flag_IsReadable,
                                                    .flag_IsWritable],
                                            valueStrings: nil,
                                            dependentParameters: nil)
        // Set default value
        parameter.value = 0.95

        return parameter
    }()

    /// Parameter to control the attack delay for the noise gate
    var gateAttackDelayParam: AUParameter = {
        let parameter =
            AUParameterTree.createParameter(withIdentifier: "gateAttackDelay",
                                            name: "Noise Gate Attack",
                                            address: RNNoise__macOS_Param.gateAttackDelay.rawValue,
                                            min: 0.0,
                                            max: 5000.0,
                                            unit: .milliseconds,
                                            unitName: nil,
                                            flags: [.flag_IsReadable,
                                                    .flag_IsWritable],
                                            valueStrings: nil,
                                            dependentParameters: nil)
        // Set default value to 100 ms, which is 10 480-sample buffers.
        parameter.value = 100.0

        return parameter
    }()

    let parameterTree: AUParameterTree
    // TODO: figure out how to deal with all of the errors that appear when I try to do this the right way.
//    let percentFormatter: NumberFormatter
//    let millisecondFormatter: MeasurementFormatter

    init(kernelAdapter: RNNoise__macOS_DSPKernelAdapter) {
//        self.percentFormatter = NumberFormatter()
//        self.percentFormatter.numberStyle = .percent
//        self.percentFormatter.multiplier = 100
//        self.millisecondFormatter = MeasurementFormatter()

        // Create the audio unit's tree of parameters
        parameterTree = AUParameterTree.createTree(withChildren: [speechConfidenceThresholdPctParam,
                                                                  gateAttackDelayParam])

        // Closure observing all externally-generated parameter value changes.
        parameterTree.implementorValueObserver = { param, value in
            kernelAdapter.setParameter(param, value: value)
        }

        // Closure returning state of requested parameter.
        parameterTree.implementorValueProvider = { param in
            return kernelAdapter.value(for: param)
        }

        // Closure returning string representation of requested parameter value.
        parameterTree.implementorStringFromValueCallback = { param, value in
            switch param.address {
            // TODO: do this with MeasurementFormatter and NumberFormatter.
            case RNNoise__macOS_Param.speechConfidenceThresholdPct.rawValue:
                return String(format: "%.2f", value ?? param.value)
            case RNNoise__macOS_Param.gateAttackDelay.rawValue:
                return String(format: "%.0f", value ?? param.value)
            default:
                return "?"
            }
        }
    }

    func setParameterValues(speechConfidenceThresholdPct: AUValue, gateAttackDelay: AUValue) {
        speechConfidenceThresholdPctParam.value = speechConfidenceThresholdPct
        gateAttackDelayParam.value = gateAttackDelay
    }
}
