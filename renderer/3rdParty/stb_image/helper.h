#pragma once

#include <stdint.h>

namespace glacier {
namespace stb_image {

void set_flip_vertically_on_load(int flag);

uint8_t* load(char const *filename, int *x, int *y, int *comp, int req_comp);
float* loadf(char const *filename, int *x, int *y, int *comp, int req_comp);

void free(void *data);

int write_png_image(char const *filename, int x, int y, int comp, const void *data, int stride_bytes);

}
}
