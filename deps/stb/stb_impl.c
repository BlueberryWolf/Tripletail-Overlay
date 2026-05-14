#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#define STBIDEF static
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

unsigned char *my_stbi_load_from_memory(unsigned char const *buffer, int len, int *x, int *y, int *channels_in_file,
                                        int desired_channels) {
    return stbi_load_from_memory(buffer, len, x, y, channels_in_file, desired_channels);
}

int my_stbi_info_from_memory(unsigned char const *buffer, int len, int *x, int *y, int *comp) {
    return stbi_info_from_memory(buffer, len, x, y, comp);
}

void my_stbi_image_free(void *retval_from_stbi_load) { stbi_image_free(retval_from_stbi_load); }

const char *my_stbi_failure_reason(void) { return stbi_failure_reason(); }
