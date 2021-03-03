#ifndef MELTDOWN_BASIC_PRIMITIVES_H
#define MELTDOWN_BASIC_PRIMITIVES_H

#include <stdint.h>

int fallout_compatible();

int ridl_init();

void ridl_cleanup();

int flush_cache(void *mem);

int lfb_read(void *mem);

#endif //MELTDOWN_BASIC_PRIMITIVES_H
