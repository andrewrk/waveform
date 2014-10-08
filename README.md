# waveform

![](http://i.imgur.com/oNy41Cr.png)

Input: any format audio or video file

Output: any or all of these:

 * transcoded audio file
 * waveform.js-compatible JSON representation of the audio file
 * PNG rendering of the waveform

## Usage

    waveform [options] in [--transcode out] [--waveformjs out] [--png out]
    (where `in` is a file path and `out` is a file path or `-` for STDOUT)

    Options:
    --scan                       duration scan (default off)

    Transcoding Options:
    --bitrate 320                audio bitrate in kbps
    --format name                e.g. mp3, ogg, mp4
    --codec name                 e.g. mp3, vorbis, flac, aac
    --mime mimetype              e.g. audio/vorbis
    --tag-artist artistname      artist tag
    --tag-title title            title tag
    --tag-year 2000              year tag
    --tag-comment comment        comment tag

    WaveformJs Options:
    --wjs-width 800              width in samples
    --wjs-precision 4            how many digits of precision
    --wjs-plain                  exclude metadata in output JSON (default off)

    PNG Options:
    --png-width 256              width of the image
    --png-height 64              height of the image
    --png-color-bg 00000000      bg color, rrggbbaa
    --png-color-center 000000ff  gradient center color, rrggbbaa
    --png-color-outer 000000ff   gradient outer color, rrggbbaa

## Installation

1. Install [libgroove](https://github.com/andrewrk/libgroove) dev package.
   Only the main library is needed.

2. Install libpng and zlib dev packages.

3. `make`

## Related Projects

 * [Node.js module](https://github.com/andrewrk/node-waveform)
 * [PHP Wrapper Script](https://github.com/polem/WaveformGenerator)
 * [Native Interface for Go](https://github.com/dz0ny/podcaster/blob/master/utils/waveform.go)
