#ifndef SCENE_H
#define SCENE_H

#include <stdbool.h>

struct scene {
    int (*logicFunc)(void);
    void (*displayFunc)(void);
    struct scene *parentScene;
    /* A touch on the bottom screen: the target's id and value, and whether the
     * finger has been lifted (released) or just touched down. Only the 3DS. */
    void (*touchFunc)(int id, int value, bool released);
    /* Called every frame when nothing else changed: an animation (3DS). */
    void (*animFunc)(void);
    /* Called when the page is left by going back (3DS). */
    void (*leaveFunc)(void);
};

extern struct scene *curScene;

#endif
