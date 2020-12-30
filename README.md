#  RNNoise For Mac

[Xiph.org](https://xiph.org)'s [RNNoise library](https://github.com/xiph/rnnoise) is wonderful. But as far as I could tell as of writing this, nobody has made an Audio Unit that does it for macOS yet. So here's my attempt at one.

I'm mostly doing this because I want to learn more about writing audio code and this seems like as good of a way as any to start learning.

## TODOs

1. ~~Figure out why linking the AU extension fails~~ (library needs `extern "C" {…}`)
2. ~~Call into rnnoise to do most of the noise reduction~~
3. Write simple AU host that can take an input device and an output device and denoise from one to the other
3. Implement noise gate using returned speech probability from librnnoise
4. Build UI to control parameters
4. Add space in AU host for the AU's UI
5. Profile for power usage and ruthlessly optimize
6. Consider replacing kissfft with Apple's FFT from the Accelerate framework
