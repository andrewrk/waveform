#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sndfile.h>
#include <png.h>

int printUsage(char * exe) {
    fprintf(stderr, "\
\n\
Usage:\n\
\n\
    waveform [options]\n\
\n\
    Available options with their defaults:\n\
    --in audiofile              (required)\n\
    --out pngfile               (required)\n\
    --width 256                 height of the image\n\
    --width 64                  width of the image\n\
    --color-bg 00000000         bg color, rrggbbaa\n\
    --color-center 000000ff     gradient center color, rrggbbaa\n\
    --color-outer 000000ff      gradient outer color, rrggbbaa\n\
\n\
");
    return 1;
}

int main(int argc, char * argv[]) {
    char * exe = argv[0];
    long image_width = 256;
    long image_height = 64;

    char * in_file_path = NULL;
    char * out_file_path = NULL;

    char color_bg[4] = {0, 0, 0, 0};
    char color_center[4] = {0, 0, 0, 255};
    char color_outer[4] = {0, 0, 0, 255};

    // parse params
    int i;
    for (i = 1; i < argc; ++i) {
        if (argv[i][0] != '-' || argv[i][1] != '-')
            return printUsage(exe);

        char * arg_name = argv[i] + 2;

        // args that take 1 parameter
        if (i + 1 >= argc)
            return printUsage(exe);
        if (strcmp(arg_name, "width") == 0) {
            image_width = atol(argv[++i]);
        } else if (strcmp(arg_name, "height") == 0) {
            image_height = atol(argv[++i]);
        } else if (strcmp(arg_name, "in") == 0) {
            in_file_path = argv[++i];
        } else if (strcmp(arg_name, "out") == 0) {
            out_file_path = argv[++i];
        } else if (strcmp(arg_name, "color-bg") == 0) {
            sscanf(argv[++i], "%x", (unsigned int *)color_bg);
        } else if (strcmp(arg_name, "color-center") == 0) {
            sscanf(argv[++i], "%x", (unsigned int *)color_center);
        } else if (strcmp(arg_name, "color-outer") == 0) {
            sscanf(argv[++i], "%x", (unsigned int *)color_outer);
        } else {
            fprintf(stderr, "Unrecognized argument: %s\n", arg_name);
            return 1;
        }
    }

    if (! in_file_path || ! out_file_path)
        return printUsage(exe);

    SF_INFO sf_info;

    SNDFILE * sndfile = sf_open(in_file_path, SFM_READ, &sf_info);

    int length = sf_info.frames / sf_info.samplerate;

    return 0;
}

