/* netmenu3ds -- the pages for playing with several consoles: choose between
 * creating and joining a group, the list of groups, and the group's lobby.
 */
#ifndef NETMENU3DS_H
#define NETMENU3DS_H

#ifdef __3DS__

#include <stdbool.h>

#include "scene.h"

/* Opens the first page. `parent` is the page that B leads back to. */
void netMenuEnter(struct scene *parent);

/* A match is over (or the host sent everybody back): the lobby again. */
void netLobbyResume(void);

/* Ends everything with a message on the first page. The host tells the others
 * first if `tellOthers` is set. */
void netMenuAbort(const char *why, bool tellOthers);

#endif

#endif
