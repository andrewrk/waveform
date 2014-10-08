#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <groove/groove.h>
#include <groove/encoder.h>
#include <pthread.h>
#include <png.h>
#include <limits.h>

// PNG stuff
static int16_t png_min_sample = INT16_MAX;
static int16_t png_max_sample = INT16_MIN;
static float y_range = (float)INT16_MAX - (float)INT16_MIN;
static float y_half_range = (float)INT16_MAX;
static int image_bound_y;
static int png_x = 0;
static png_bytep *row_pointers;
static png_byte color_bg[4] = {0, 0, 0, 0};
static png_byte color_center[4] = {0, 0, 0, 255};
static png_byte color_outer[4] = {0, 0, 0, 255};
static png_bytep color_at_pix;
static int png_width = 256;
static int png_height = 64;
static int png_frames_per_pixel;
static int png_frames_until_emit;

// transcoding stuff
static FILE *transcode_out_f = NULL;
static struct GrooveEncoder *encoder = NULL;

static int version() {
    printf("2.0.0\n");
    return 0;
}

static int usage(const char *exe) {
    fprintf(stderr, "\
\n\
Usage:\n\
\n\
waveform [options] in [--transcode out] [--waveformjs out] [--png out]\n\
(where `in` is a file path and `out` is a file path or `-` for STDOUT)\n\
\n\
Options:\n\
--scan                       duration scan (default off)\n\
\n\
Transcoding Options:\n\
--bitrate 320                audio bitrate in kbps\n\
--format name                e.g. mp3, ogg, mp4\n\
--codec name                 e.g. mp3, vorbis, flac, aac\n\
--mime mimetype              e.g. audio/vorbis\n\
--tag-artist artistname      artist tag\n\
--tag-title title            title tag\n\
--tag-year 2000              year tag\n\
--tag-comment comment        comment tag\n\
\n\
WaveformJs Options:\n\
--wjs-width 800              width in samples\n\
--wjs-precision 4            how many digits of precision\n\
--wjs-plain                  exclude metadata in output JSON (default off)\n\
\n\
PNG Options:\n\
--png-width 256              width of the image\n\
--png-height 64              height of the image\n\
--png-color-bg 00000000      bg color, rrggbbaa\n\
--png-color-center 000000ff  gradient center color, rrggbbaa\n\
--png-color-outer 000000ff   gradient outer color, rrggbbaa\n\
\n");

    return 1;
}

static void *encode_write_thread(void *arg) {
    struct GrooveBuffer *buffer;

    while (groove_encoder_buffer_get(encoder, &buffer, 1) == GROOVE_BUFFER_YES) {
        fwrite(buffer->data[0], 1, buffer->size, transcode_out_f);
        groove_buffer_unref(buffer);
    }
    return NULL;
}

static int16_t int16_abs(int16_t x) {
    return x < 0 ? -x : x;
}

static int double_ceil(double x) {
    int n = x;
    return (x == (double)n) ? n : n + 1;
}

static void parseColor(const char * hex_str, png_bytep color) {
    unsigned long value = strtoul(hex_str, NULL, 16);
    color[3] = value & 0xff; value >>= 8;
    color[2] = value & 0xff; value >>= 8;
    color[1] = value & 0xff; value >>= 8;
    color[0] = value & 0xff;
}

static void emit_png_column() {
    // translate into y pixel coord.
    float float_min_sample = (float)png_min_sample;
    float float_max_sample = (float)png_max_sample;
    int y_min = ((float_min_sample + y_half_range) / y_range) * image_bound_y;
    int y_max = ((float_max_sample + y_half_range) / y_range) * image_bound_y;

    int y = 0;
    int four_x = 4 * png_x;

    // top bg
    for (; y < y_min; ++y) {
        memcpy(row_pointers[y] + four_x, color_bg, 4);
    }
    // top and bottom wave
    for (; y <= y_max; ++y) {
        memcpy(row_pointers[y] + four_x, color_at_pix + 4*y, 4);
    }
    // bottom bg
    for (; y < png_height; ++y) {
        memcpy(row_pointers[y] + four_x, color_bg, 4);
    }

    png_x += 1;
    png_frames_until_emit = png_frames_per_pixel;
    png_max_sample = INT16_MIN;
    png_min_sample = INT16_MAX;
}

int main(int argc, char * argv[]) {
    // arg parsing
    char *exe = argv[0];

    char *input_filename = NULL;
    char *transcode_output = NULL;
    char *waveformjs_output = NULL;
    char *png_output = NULL;

    int bit_rate_k = 320;
    char *format = NULL;
    char *codec = NULL;
    char *mime = NULL;
    char *tag_artist = NULL;
    char *tag_title = NULL;
    char *tag_year = NULL;
    char *tag_comment = NULL;

    int wjs_width = 800;
    int wjs_precision = 4;
    int wjs_plain = 0;

    int scan = 0;

    int i;
    for (i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            arg += 2;
            if (strcmp(arg, "scan") == 0) {
                scan = 1;
            } else if (strcmp(arg, "wjs-plain") == 0) {
                wjs_plain = 1;
            } else if (strcmp(arg, "version") == 0) {
                return version();
            } else if (i + 1 >= argc) {
                // args that take 1 parameter
                return usage(exe);
            } else if (strcmp(arg, "png") == 0) {
                png_output = argv[++i];
            } else if (strcmp(arg, "png-width") == 0) {
                png_width = atoi(argv[++i]);
            } else if (strcmp(arg, "png-height") == 0) {
                png_height = atoi(argv[++i]);
            } else if (strcmp(arg, "png-color-bg") == 0) {
                parseColor(argv[++i], color_bg);
            } else if (strcmp(arg, "png-color-center") == 0) {
                parseColor(argv[++i], color_center);
            } else if (strcmp(arg, "png-color-outer") == 0) {
                parseColor(argv[++i], color_outer);
            } else if (strcmp(arg, "bitrate") == 0) {
                bit_rate_k = atoi(argv[++i]);
            } else if (strcmp(arg, "format") == 0) {
                format = argv[++i];
            } else if (strcmp(arg, "codec") == 0) {
                codec = argv[++i];
            } else if (strcmp(arg, "mime") == 0) {
                mime = argv[++i];
            } else if (strcmp(arg, "transcode") == 0) {
                transcode_output = argv[++i];
            } else if (strcmp(arg, "waveformjs") == 0) {
                waveformjs_output = argv[++i];
            } else if (strcmp(arg, "tag-artist") == 0) {
                tag_artist = argv[++i];
            } else if (strcmp(arg, "tag-title") == 0) {
                tag_title = argv[++i];
            } else if (strcmp(arg, "tag-year") == 0) {
                tag_year = argv[++i];
            } else if (strcmp(arg, "tag-comment") == 0) {
                tag_comment = argv[++i];
            } else if (strcmp(arg, "wjs-width") == 0) {
                wjs_width = atoi(argv[++i]);
            } else if (strcmp(arg, "wjs-precision") == 0) {
                wjs_precision = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Unrecognized argument: %s\n", arg);
                return usage(exe);
            }
        } else if (!input_filename) {
            input_filename = arg;
        } else {
            fprintf(stderr, "Unexpected parameter: %s\n", arg);
            return usage(exe);
        }
    }

    if (!input_filename) {
        fprintf(stderr, "input file parameter required\n");
        return usage(exe);
    }

    if (!transcode_output && !waveformjs_output && !png_output) {
        fprintf(stderr, "at least one output required\n");
        return usage(exe);
    }

    // arg parsing done. let's begin.

    groove_init();
    atexit(groove_finish);

    struct GrooveFile *file = groove_file_open(input_filename);
    if (!file) {
        fprintf(stderr, "Error opening input file: %s\n", input_filename);
        return 1;
    }
    struct GroovePlaylist *playlist = groove_playlist_create();
    groove_playlist_set_fill_mode(playlist, GROOVE_ANY_SINK_FULL);

    struct GrooveSink *sink = groove_sink_create();
    sink->audio_format.sample_rate = 44100;
    sink->audio_format.channel_layout = GROOVE_CH_LAYOUT_STEREO;
    sink->audio_format.sample_fmt = GROOVE_SAMPLE_FMT_S16;

    if (groove_sink_attach(sink, playlist) < 0) {
        fprintf(stderr, "error attaching sink\n");
        return 1;
    }

    struct GrooveBuffer *buffer;
    int frame_count;

    // scan the song for the exact correct frame count and duration
    float duration;
    if (scan) {
        struct GroovePlaylistItem *item = groove_playlist_insert(playlist, file, 1.0, 1.0, NULL);
        frame_count = 0;
        while (groove_sink_buffer_get(sink, &buffer, 1) == GROOVE_BUFFER_YES) {
            frame_count += buffer->frame_count;
            groove_buffer_unref(buffer);
        }
        groove_playlist_remove(playlist, item);
        duration = frame_count / 44100.0f;
    } else {
        duration = groove_file_duration(file);
        frame_count = double_ceil(duration * 44100.0);
    }

    pthread_t thread_id;
    if (transcode_output) {
        if (strcmp(transcode_output, "-") == 0) {
            transcode_out_f = stdout;
        } else {
            transcode_out_f = fopen(transcode_output, "wb");
            if (!transcode_out_f) {
                fprintf(stderr, "Error opening output file %s\n", transcode_output);
                return 1;
            }
        }

        encoder = groove_encoder_create();
        encoder->bit_rate = bit_rate_k * 1000;
        encoder->format_short_name = format;
        encoder->codec_short_name = codec;
        encoder->filename = transcode_output;
        encoder->mime_type = mime;
        if (tag_artist)
            groove_encoder_metadata_set(encoder, "artist", tag_artist, 0);
        if (tag_title)
            groove_encoder_metadata_set(encoder, "title", tag_title, 0);
        if (tag_year)
            groove_encoder_metadata_set(encoder, "year", tag_year, 0);
        if (tag_comment)
            groove_encoder_metadata_set(encoder, "comment", tag_comment, 0);

        encoder->target_audio_format.sample_rate = 44100;
        encoder->target_audio_format.channel_layout = GROOVE_CH_LAYOUT_STEREO;
        encoder->target_audio_format.sample_fmt = GROOVE_SAMPLE_FMT_S16;

        if (groove_encoder_attach(encoder, playlist) < 0) {
            fprintf(stderr, "error attaching encoder\n");
            return 1;
        }

        // this thread pops encoded audio buffers from the sink and writes them
        // to the output
        // we'll use the main thread for waveformjs/png output
        pthread_create(&thread_id, NULL, encode_write_thread, NULL);
    }

    // insert the item *after* creating the sinks to avoid race conditions
    groove_playlist_insert(playlist, file, 1.0, 1.0, NULL);

    int png_center_y;
    double png_center_y_float;
    FILE *png_file = NULL;
    png_structp png = NULL;
    png_infop png_info = NULL;
    if (png_output) {
        png_frames_per_pixel = frame_count / png_width;
        if (png_frames_per_pixel < 1) png_frames_per_pixel = 1;

        png_center_y = png_height / 2;
        png_center_y_float = (double) png_center_y;

        png_frames_until_emit = png_frames_per_pixel;

        if (strcmp(png_output, "-") == 0) {
            png_file = stdout;
        } else {
            png_file = fopen(png_output, "wb");
            if (!png_file) {
                fprintf(stderr, "Unable to open %s for writing\n", png_output);
                return 1;
            }
        }

        png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png) {
            fprintf(stderr, "Unable to allocate png write structure\n");
            return 1;
        }

        png_info = png_create_info_struct(png);

        if (! png_info) {
            fprintf(stderr, "Unable to allocate png info structure\n");
            return 1;
        }

        png_init_io(png, png_file);

        png_set_IHDR(png, png_info, png_width, png_height, 8,
            PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

        png_write_info(png, png_info);

        // allocate memory to write to png file
        row_pointers = (png_bytep *) malloc(sizeof(png_bytep) * png_height);

        if (! row_pointers) {
            fprintf(stderr, "Out of memory.\n");
            return 1;
        }

        color_at_pix = (png_bytep) malloc(sizeof(png_byte) * png_height * 4);

        if (! color_at_pix) {
            fprintf(stderr, "Out of memory.\n");
            return 1;
        }

        int y;
        for (y = 0; y < png_height; ++y) {
            png_bytep row = (png_bytep) malloc(png_width * 4);
            if (! row) {
                fprintf(stderr, "Out of memory.\n");
                return 1;
            }
            row_pointers[y] = row;

            // compute the foreground color at each y pixel
            int i;
            for (i = 0; i < 4; ++i) {
                double amt = abs(y - png_center_y) / png_center_y_float;
                color_at_pix[4*y + i] = (1-amt) * color_center[i] + amt * color_outer[i];
            }
        }

        image_bound_y = png_height - 1;
    }

    FILE *waveformjs_f = NULL;
    int wjs_frames_per_pixel = 0;
    int16_t wjs_max_sample;
    int wjs_frames_until_emit = 0;
    int wjs_emit_count;
    if (waveformjs_output) {
        if (strcmp(waveformjs_output, "-") == 0) {
            waveformjs_f = stdout;
        } else {
            waveformjs_f = fopen(waveformjs_output, "wb");
            if (!waveformjs_f) {
                fprintf(stderr, "Error opening output file: %s\n", waveformjs_output);
                return 1;
            }
        }

        wjs_frames_per_pixel = frame_count / wjs_width;
        if (wjs_frames_per_pixel < 1)
            wjs_frames_per_pixel = 1;

        if (!wjs_plain) {
            fprintf(waveformjs_f, "{\"frameCount\":%d,\"frameRate\":%d, \"waveformjs\":",
                    frame_count, 44100);
        }

        fprintf(waveformjs_f, "[");

        wjs_max_sample = INT16_MIN;
        wjs_frames_until_emit = wjs_frames_per_pixel;
        wjs_emit_count = 0;
    }

    while (groove_sink_buffer_get(sink, &buffer, 1) == GROOVE_BUFFER_YES) {
        if (png_output) {
            int i;
            for (i = 0; i < buffer->frame_count && png_x < png_width;
                    i += 1, png_frames_until_emit -= 1)
            {
                if (png_frames_until_emit == 0) {
                    emit_png_column();
                }
                int16_t *samples = (int16_t *) buffer->data[0];
                int16_t left = samples[i];
                int16_t right = samples[i + 1];
                int16_t avg = (left + right) / 2;

                if (avg > png_max_sample) png_max_sample = avg;
                if (avg < png_min_sample) png_min_sample = avg;
            }
        }
        if (waveformjs_output) {
            int i;
            for (i = 0; i < buffer->frame_count && wjs_emit_count < wjs_width;
                    i += 1, wjs_frames_until_emit -= 1)
            {
                if (wjs_frames_until_emit == 0) {
                    wjs_emit_count += 1;
                    char *comma = (wjs_emit_count == wjs_width) ? "" : ",";
                    double float_sample = wjs_max_sample / (double) INT16_MAX;
                    fprintf(waveformjs_f, "%.*f%s", wjs_precision, float_sample, comma);
                    wjs_max_sample = INT16_MIN;
                    wjs_frames_until_emit = wjs_frames_per_pixel;
                }
                int16_t *data = (int16_t *) buffer->data[0];
                int16_t *left = &data[i];
                int16_t *right = &data[i + 1];
                int16_t abs_left = int16_abs(*left);
                int16_t abs_right = int16_abs(*right);
                if (abs_left > wjs_max_sample) wjs_max_sample = abs_left;
                if (abs_right > wjs_max_sample) wjs_max_sample = abs_right;
            }
        }

        groove_buffer_unref(buffer);
    }

    if (png_output) {
        // emit the last column if necessary. This will have to run multiple times
        // if the duration specified in the metadata is incorrect.
        while (png_x < png_width) {
            emit_png_column();
        }

        png_write_image(png, row_pointers);
        png_write_end(png, png_info);
        fclose(png_file);
    }

    if (waveformjs_output) {
        if (wjs_emit_count < wjs_width) {
            // emit the last sample
            double float_sample = wjs_max_sample / (double) INT16_MAX;
            fprintf(waveformjs_f, "%.*f", wjs_precision, float_sample);
        }

        fprintf(waveformjs_f, "]");

        if (!wjs_plain)
            fprintf(waveformjs_f, "}");

        fclose(waveformjs_f);
    }

    if (transcode_output) {
        pthread_join(thread_id, NULL);
        fclose(transcode_out_f);

        groove_encoder_detach(encoder);
        groove_encoder_destroy(encoder);
    }


    groove_sink_detach(sink);
    groove_sink_destroy(sink);

    groove_playlist_clear(playlist);
    groove_file_close(file);
    groove_playlist_destroy(playlist);

    return 0;
}
