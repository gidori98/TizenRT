#ifndef __INCLUDE_TINYARA_GIDORI_H
#define __INCLUDE_TINYARA_GIDORI_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>
#include <stdint.h>

#define GIDORI_WRITE     0
#define GIDORI_READ      1


#define BUFFER_SIZE 128

struct gidori_dev_buf{
    char buffer[BUFFER_SIZE];
};

void gidori_register(FAR struct gidori_dev_buf *dev);

#endif
