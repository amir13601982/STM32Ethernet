#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

#define htons(x) ((uint16_t)( (((uint16_t)(x)) << 8) | \
                               (((uint16_t)(x)) >> 8) ))
#define ntohs(x) htons(x)


#define htonl(x) ((uint32_t)(                           \
    ((((uint32_t)(x)) << 24) & 0xFF000000UL) |          \
    ((((uint32_t)(x)) <<  8) & 0x00FF0000UL) |          \
    ((((uint32_t)(x)) >>  8) & 0x0000FF00UL) |          \
    ((((uint32_t)(x)) >> 24) & 0x000000FFUL)            \
))
#define ntohl(x) htonl(x)

#endif // UTIL_H
