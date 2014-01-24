# waveform

![](http://i.imgur.com/oNy41Cr.png)

## Command Line Usage:

    waveform [options] audiofile pngfile

    Available options with their defaults:
    --width 256                 width of the image
    --height 64                 height of the image
    --color-bg 00000000         bg color, rrggbbaa
    --color-center 000000ff     gradient center color, rrggbbaa
    --color-outer 000000ff      gradient outer color, rrggbbaa

    Substitute '-' for a filename to use stdio.

## Node.js Usage:

```js
var generateWaveform = require('waveform');
generateWaveform(audiofile, pngfile, {
    width: 256,                // width of the image
    height: 64,                // height of the image
    'color-bg': '00000000',    // bg color, rrggbbaa
    'color-center': '000000ff',// gradient center color, rrggbbaa
    'color-outer': '000000ff', // gradient outer color, rrggbbaa
}, function(err) {
    // done generating waveform
});
```

## PHP Usage:

https://github.com/polem/WaveformGenerator

## Dependencies:

 * [libgroove](https://github.com/andrewrk/libgroove)
 * libpng
 * zlib

## Compile:

    gcc -o waveform main.c -O3 -lgroove -lz -lpng
