#ifndef INCLUDE_GENTEST_H
#define INCLUDE_GENTEST_H

int gentest_init(unsigned int width, unsigned int height, unsigned int stride,
		 unsigned int crop_w, unsigned int crop_h,
		 unsigned int size);
void gentest_deinit(void);
void gentest_fill(unsigned int width, unsigned int height, unsigned char *to,
		  unsigned int size);

#endif /* INCLUDE_GENTEST_H */