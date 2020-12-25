#  RNNoise For Mac

[Xiph.org](https://xiph.org)'s [RNNoise library](https://github.com/xiph/rnnoise) is wonderful. But as far as I could tell as of writing this, nobody has made an Audio Unit that does it for macOS yet. So here's my attempt at one.

I'm mostly doing this because I want to learn more about writing audio code and this seems like as good of a way as any to start learning.

## TODOs

1. Figure out why linking the AU extension fails
2. Call into rnnoise to calculate speech probability
3. Implement noise reduction using parameters
4. Build UI to control parameters
5. Profile for power usage and ruthlessly optimize
