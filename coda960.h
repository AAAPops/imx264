#ifndef INCLUDE_CODA960_H
#define INCLUDE_CODA960_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>


struct Coda_inst {
    char             coda_name[128];
    int              coda_fd;
};


#endif /* INCLUDE_CODA960_H */