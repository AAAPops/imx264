#include <stdio.h>
#include <string.h>

#include "arm_neon.h"

void add3 (uint8x16_t *data) {
    /* Set each sixteen values of the vector to 3.
     *
     * Remark: a 'q' suffix to intrinsics indicates
     * the instruction run for 128 bits registers.
     */
    uint8x16_t three = vmovq_n_u8 (3);

    /* Add 3 to the value given in argument. */
    *data = vaddq_u8 (*data, three);
}

void print_uint8 (uint8x16_t data, char* name) {
    int i;
    static uint8_t p[16];

    vst1q_u8 (p, data);

    printf ("%s = ", name);
    for (i = 0; i < 16; i++) {
	printf ("%02d ", p[i]);
    }
    printf ("\n");
}

void print_arr(uint8_t *arr_ptr, int len, char *name) {
    int i;

    printf ("%s \n", name);
    for (i = 0; i < len; i++) {
        printf ("%d ", arr_ptr[i]);
    }
    printf ("\n");
}

int main () {
    /* Create custom arbitrary data. */
    uint8_t in_data[UINT8_MAX];
    uint8_t out_data_1[UINT8_MAX];
    uint8_t out_data_2[UINT8_MAX];

    /* Create the vector with our data. */
    uint8x16x2_t data;

    int iter;
    for(iter = 0; iter < UINT8_MAX; iter++){
        in_data[iter] = iter;
    }
    print_arr(in_data, UINT8_MAX, "Input data:");

    memset(out_data_1, 0, sizeof(out_data_1));
    memset(out_data_2, 0, sizeof(out_data_2));

    //int num8x16 = width *height / 16;
    uint8x16x2_t uint_8x16x2;
    for (int i=0; i < UINT8_MAX / (2*16); i++) {
        uint_8x16x2 = vld2q_u8(in_data + 2*16*i);
        vst1q_u8(out_data_1 + 16*i, uint_8x16x2.val[0]);
        vst1q_u8(out_data_2 + 16*i, uint_8x16x2.val[1]);

    }

    print_arr(out_data_1, UINT8_MAX, "Output data-1:");
    print_arr(out_data_2, UINT8_MAX, "Output data-2:");


    return 0;
}


