#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <groove/groove.h>
#include <png.h>
#include <limits.h>

static float min_sample = 1.0f;
static float max_sample = -1.0f;
static int image_bound_y;
static int x = 0;
static png_bytep *row_pointers;
static png_byte color_bg[4] = {0, 0, 0, 0};
static png_byte color_center[4] = {0, 0, 0, 255};
static png_byte color_outer[4] = {0, 0, 0, 255};
static png_bytep color_at_pix;
static int image_width = 256;
static int image_height = 64;
static int frames_per_pixel;
static int frames_until_emit;

static int printUsage(const char * exe) {
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
\n\
");
    return 1;
}

static void parseColor(const char * hex_str, png_bytep color) {
    unsigned long value = strtoul(hex_str, NULL, 16);
    color[3] = value & 0xff; value >>= 8;
    color[2] = value & 0xff; value >>= 8;
    color[1] = value & 0xff; value >>= 8;
    color[0] = value & 0xff;
}

static int double_ceil(double x) {
    int n = x;
    return (x == (double)n) ? n : n + 1;
}

static void emit_column() {
    // translate into y pixel coord.
    int y_min = (min_sample + 1.0f) / 2.0f * image_bound_y;
    int y_max = (max_sample + 1.0f) / 2.0f * image_bound_y;

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

    x += 1;
    frames_until_emit = frames_per_pixel;
    max_sample = -1.0f;
    min_sample = 1.0f;
}

int main(int argc, char * argv[]) {
    char * exe = argv[0];

    char * in_file_path = NULL;
    char * out_file_path = NULL;

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
                image_width = atoi(argv[++i]);
            } else if (strcmp(arg_name, "height") == 0) {
                image_height = atoi(argv[++i]);
            } else if (strcmp(arg_name, "color-bg") == 0) {
                parseColor(argv[++i], color_bg);
            } else if (strcmp(arg_name, "color-center") == 0) {
                parseColor(argv[++i], color_center);
            } else if (strcmp(arg_name, "color-outer") == 0) {
                parseColor(argv[++i], color_outer);
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

    groove_init();
    atexit(groove_finish);

    struct GrooveFile *file = groove_file_open(in_file_path);
    if (! file) {
        fprintf(stderr, "Error opening input file: %s\n", in_file_path);
        return 1;
    }
    struct GroovePlaylist *playlist = groove_playlist_create();

    struct GrooveSink *sink = groove_sink_create();
    sink->audio_format.sample_rate = 44100;
    sink->audio_format.channel_layout = GROOVE_CH_LAYOUT_MONO;
    sink->audio_format.sample_fmt = GROOVE_SAMPLE_FMT_FLT;

    if (groove_sink_attach(sink, playlist) < 0) {
        fprintf(stderr, "error attaching sink\n");
        return 1;
    }

    groove_playlist_insert(playlist, file, 1.0, NULL);

    struct GrooveBuffer *buffer;

    float duration = groove_file_duration(file);
    int frame_count = double_ceil(duration * 44100.0);
    frames_per_pixel = frame_count / image_width;
    if (frames_per_pixel < 1) frames_per_pixel = 1;

    int center_y = image_height / 2;

    frames_until_emit = frames_per_pixel;


    FILE * png_file;
    if (strcmp(out_file_path, "-") == 0) {
        png_file = stdout;
    } else {
        png_file = fopen(out_file_path, "wb");
        if (!png_file) {
            fprintf(stderr, "Unable to open %s for writing\n", out_file_path);
            return 1;
        }
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

    // allocate memory to write to png file
    row_pointers = (png_bytep *) malloc(sizeof(png_bytep) * image_height);

    if (! row_pointers) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }

    color_at_pix = (png_bytep) malloc(sizeof(png_byte) * image_height * 4);

    if (! color_at_pix) {
        fprintf(stderr, "Out of memory.\n");
        return 1;
    }

    int y;
    double center_y_float = (double) center_y;
    for (y = 0; y < image_height; ++y) {
        png_bytep row = (png_bytep) malloc(image_width * 4);
        if (! row) {
            fprintf(stderr, "Out of memory.\n");
            return 1;
        }
        row_pointers[y] = row;

        // compute the foreground color at each y pixel
        int i;
        for (i = 0; i < 4; ++i) {
            double amt = abs(y - center_y) / center_y_float;
            color_at_pix[4*y + i] = (1-amt) * color_center[i] + amt * color_outer[i];
        }
    }

    image_bound_y = image_height - 1;
    while (groove_sink_buffer_get(sink, &buffer, 1) == GROOVE_BUFFER_YES) {
        // process the buffer
        for (i = 0; i < buffer->frame_count && x < image_width;
                i += 1, frames_until_emit -= 1)
        {
            if (frames_until_emit == 0) {
                emit_column();
            }
            float *samples = (float *) buffer->data[0];
            float sample = samples[i];
            if (sample > max_sample) max_sample = sample;
            if (sample < min_sample) min_sample = sample;
        }

        groove_buffer_unref(buffer);
    }

    if (x < image_width)  {
        // emit the last column
        emit_column();
    }

    png_write_image(png, row_pointers);
    png_write_end(png, png_info);
    fclose(png_file);

    groove_sink_detach(sink);
    groove_sink_destroy(sink);

    groove_playlist_clear(playlist);
    groove_file_close(file);
    groove_playlist_destroy(playlist);

    return 0;
}
