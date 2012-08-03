#include <stdio.h>
#include <sndfile.h>
#include <png.h>

int main(int argc, char * argv[] ) {
    if (argc != 3) {
        printf("Usage: \n\n%s audio waveform.png\n\n", argv[0]);
        return 1;
    }

    char * wav_file_path = argv[1];
    char * png_file_path = argv[2];

    SF_INFO sf_info;

    SNDFILE * sndfile = sf_open(wav_file_path, SFM_READ, &sf_info);

    int length = sf_info.frames / sf_info.samplerate;

    return 0;
}
