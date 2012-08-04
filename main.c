#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sndfile.h>
#include <mpg123.h>
#include <png.h>
#include <limits.h>

int printUsage(const char * exe) {
    fprintf(stderr, "\
\n\
Usage:\n\
\n\
    waveform [options] audiofile pngfile\n\
\n\
    Available options with their defaults:\n\
    --width 256                 width of the image\n\
    --height 64                 height of the image\n\
    --color-bg 00000000         bg color, rrggbbaa\n\
    --color-center 000000ff     gradient center color, rrggbbaa\n\
    --color-outer 000000ff      gradient outer color, rrggbbaa\n\
    --quality 100               integer 1-100 choose between speed and quality\n\
\n\
");
    return 1;
}

void parseColor(const char * hex_str, png_bytep color) {
    unsigned long value = strtoul(hex_str, NULL, 16);
    color[3] = value & 0xff; value >>= 8;
    color[2] = value & 0xff; value >>= 8;
    color[1] = value & 0xff; value >>= 8;
    color[0] = value & 0xff;
}

int main(int argc, char * argv[]) {
    char * exe = argv[0];
    long image_width = 256;
    long image_height = 64;
    int quality = 100;

    char * in_file_path = NULL;
    char * out_file_path = NULL;

    png_byte color_bg[4] = {0, 0, 0, 0};
    png_byte color_center[4] = {0, 0, 0, 255};
    png_byte color_outer[4] = {0, 0, 0, 255};

    // parse params
    int i;
    for (i = 1; i < argc; ++i) {
        if (argv[i][0] != '-' || argv[i][1] != '-') {
            if (! in_file_path) {
                in_file_path = argv[i];
            } else if (! out_file_path) {
                out_file_path = argv[i];
            } else {
                fprintf(stderr, "don't know what to do with param: %s\n", argv[i]);
                return printUsage(exe);
            }
        } else {
            char * arg_name = argv[i] + 2;

            // args that take 1 parameter
            if (i + 1 >= argc)
                return printUsage(exe);
            if (strcmp(arg_name, "width") == 0) {
                image_width = atol(argv[++i]);
            } else if (strcmp(arg_name, "height") == 0) {
                image_height = atol(argv[++i]);
            } else if (strcmp(arg_name, "color-bg") == 0) {
                parseColor(argv[++i], color_bg);
            } else if (strcmp(arg_name, "color-center") == 0) {
                parseColor(argv[++i], color_center);
            } else if (strcmp(arg_name, "color-outer") == 0) {
                parseColor(argv[++i], color_outer);
            } else if (strcmp(arg_name, "quality") == 0) {
                quality = atoi(argv[++i]);
                if (quality < 1 || quality > 100) {
                    fprintf(stderr, "quality must be an integer between 1 and 10\n");
                    return printUsage(exe);
                }
            } else {
                fprintf(stderr, "Unrecognized argument: %s\n", arg_name);
                return printUsage(exe);
            }
        }
    }

    if (! in_file_path || ! out_file_path) {
        fprintf(stderr, "input and output file parameters required\n");
        return printUsage(exe);
    }

    // try libsndfile, then libmpg123, then give up
    const int LIB_SNDFILE = 0;
    const int LIB_MPG123 = 1;
    int lib_to_use = LIB_SNDFILE;

    int frame_count = -1;
    int channel_count = -1;
    int frame_size = -1;

    // libsndfile variables
    SF_INFO sf_info;

    // mpg123 variables
    mpg123_handle * mh = NULL;

    // try libsndfile
    SNDFILE * sndfile = sf_open(in_file_path, SFM_READ, &sf_info);

    if (sndfile) {
        // sndfile can handle the file
        frame_count = sf_info.frames;
        channel_count = sf_info.channels;
    } else {
        // sndfile no good. let's try mpg123.
        int err = mpg123_init();
        int encoding = 0;
        long rate = 0;
        if (err != MPG123_OK || (mh = mpg123_new(NULL, &err)) == NULL
            // let mpg123 work with the file, that excludes MPG123_NEED_MORE
            // messages.
            || mpg123_open(mh, in_file_path) != MPG123_OK
            // peek into the track and get first output format.
            || mpg123_getformat(mh, &rate, &channel_count, &encoding) != MPG123_OK)
        {
            fprintf(stderr, "Unrecognized audio format for input file: %s\n", in_file_path);
            return 1;
        }

        mpg123_format_none(mh);
        mpg123_format(mh, rate, channel_count, encoding);

        // mpg123 can handle the file
        lib_to_use = LIB_MPG123;
        mpg123_scan(mh);
        int sample_size = 2;
        frame_size = sample_size * channel_count;
        int sample_count = mpg123_length(mh);
        frame_count = sample_count / channel_count;
    }


    int sample_min = SHRT_MIN;
    int sample_max = SHRT_MAX;
    int sample_range = sample_max - sample_min;

    int center_y = image_height / 2;

    FILE * png_file = fopen(out_file_path, "wb");
    if (!png_file) {
        fprintf(stderr, "Unable to open %s for writing\n", out_file_path);
        return 1;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (! png) {
        fprintf(stderr, "Unable to allocate png write structure\n");
        return 1;
    }

    png_infop png_info = png_create_info_struct(png);

    if (! png_info) {
        fprintf(stderr, "Unable to allocate png info structure\n");
        return 1;
    }

    png_init_io(png, png_file);

    png_set_IHDR(png, png_info, image_width, image_height, 8,
        PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png, png_info);

    int frames_per_pixel = frame_count / image_width;
    int frames_to_see = frames_per_pixel * quality / 100;
    int frames_times_channels = frames_to_see * channel_count;

    // allocate memory to read from library
    short * frames = (short *) malloc(sizeof(short) * channel_count * frames_to_see);
    if (! frames) {
        fprintf(stderr, "Out of memory.");
        return 1;
    }

    // allocate memory to write to png file
    png_bytep * row_pointers = (png_bytep *) malloc(sizeof(png_bytep) * image_height);

    if (! row_pointers) {
        fprintf(stderr, "Out of memory.");
        return 1;
    }

    png_bytep color_at_pix = (png_bytep) malloc(sizeof(png_byte) * image_height * 4);

    if (! color_at_pix) {
        fprintf(stderr, "Out of memory.");
        return 1;
    }

    int y;
    float center_y_float = (float) center_y;
    for (y = 0; y < image_height; ++y) {
        png_bytep row = (png_bytep) malloc(image_width * 4);
        if (! row) {
            fprintf(stderr, "Out of memory.");
            return 1;
        }
        row_pointers[y] = row;

        // compute the foreground color at each y pixel
        int i;
        for (i = 0; i < 4; ++i) {
            float amt = abs(y - center_y) / center_y_float;
            color_at_pix[4*y + i] = (1-amt) * color_center[i] + amt * color_outer[i];
        }
    }

    // for each pixel
    int image_bound_y = image_height - 1;
    int x;
    float channel_count_mult = 1 / (float)channel_count;
    for (x = 0; x < image_width; ++x) {
        // range of frames that fit in this pixel
        int start = x * frames_per_pixel;

        // get the min and max of this range
        int min = sample_max;
        int max = sample_min;

        if (lib_to_use == LIB_SNDFILE) {
            sf_seek(sndfile, start, SEEK_SET);
            sf_readf_short(sndfile, frames, frames_to_see);
        } else if (lib_to_use == LIB_MPG123) {
            size_t done;
            mpg123_seek(mh, start * channel_count, SEEK_SET);
            mpg123_read(mh, (unsigned char *) frames, frames_to_see * frame_size, &done);
        }

        // for each frame from start to end
        int i;
        for (i = 0; i < frames_times_channels; i += channel_count) {
            // average the channels
            int value = 0;
            int c;
            for (c = 0; c < channel_count; ++c) {
                value += frames[i+c] * channel_count_mult;
            }

            // keep track of max/min
            if (value < min) min = value;
            if (value > max) max = value;
        }
        // translate into y pixel coord.
        int y_min = (min - sample_min) * image_bound_y / sample_range;
        int y_max = (max - sample_min) * image_bound_y / sample_range;

        int y = 0;
        int four_x = 4 * x;

        // top bg 
        for (; y < y_min; ++y) {
            memcpy(row_pointers[y] + four_x, color_bg, 4);
        }
        // top and bottom wave
        for (; y <= y_max; ++y) {
            memcpy(row_pointers[y] + four_x, color_at_pix + 4*y, 4);
        }
        // bottom bg
        for (; y < image_height; ++y) {
            memcpy(row_pointers[y] + four_x, color_bg, 4);
        }
    }

    png_write_image(png, row_pointers);
    png_write_end(png, png_info);
    fclose(png_file);

    if (lib_to_use == LIB_SNDFILE) {
        sf_close(sndfile);
    } else if (lib_to_use == LIB_MPG123) {
        mpg123_close(mh);
        mpg123_delete(mh);
        mpg123_exit();
    }

    // let the OS free up the memory that we allocated
    return 0;
}
