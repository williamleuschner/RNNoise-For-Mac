//
//  ContentView.swift
//  RNNoise For Mac
//
//  Created by William on 24 Dec.
//

import SwiftUI

struct ContentView: View {
    @State var denoisingEnabled: Bool = false
    @State var inputDevice: Int = 0
    @State var outputDevice: Int = 0

    var body: some View {
        VStack(alignment: .leading) {
            Picker("Input Device:", selection: $inputDevice) {
                Text("Default").tag(0)
                Text("Focusrite Scarlett 2i2").tag(1)
            }

            Picker("Output Device:", selection: $outputDevice) {
                Text("Default").tag(0)
                Text("MacBook Pro Speakers").tag(1)
                Text("Blackhole 2ch").tag(2)
            }

            Toggle("Denoising enabled", isOn: $denoisingEnabled)
                // This is a hack.
                .padding(.leading, 99)

            Text("Placeholder for AU UI")
                .padding(100)
                .background(Color.init(NSColor.systemGray))
        }
        .padding()
        .frame(minWidth: 365)
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView().frame(width: 365, height: 330)
    }
}
