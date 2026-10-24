/* netmenu3ds -- the pages for playing with several consoles: choose between
 * creating and joining a group, the list of groups, and the group's lobby.
 */
#ifndef NETMENU3DS_H
#define NETMENU3DS_H

#ifdef __3DS__

#include "scene.h"

/* Opens the first page. `parent` is the page that B leads back to. */
void netMenuEnter(struct scene *parent);

#endif

#endif
