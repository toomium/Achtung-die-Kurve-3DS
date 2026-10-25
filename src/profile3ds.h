/* profile3ds -- the page with the numbers of the stats and the titles. */
#ifndef PROFILE3DS_H
#define PROFILE3DS_H

#ifdef __3DS__

#include "scene.h"

/* Opens the page; `parent` is where B leads back to. */
void profileEnter(struct scene *parent);

#endif

#endif
