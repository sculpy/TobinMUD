/*******************************************************************
 * TobinMUD Client ver. 0.1                                        *
 *******************************************************************/
#include "map_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const MAP_DIR_NAMES[MAP_NUM_EXITS] = {
    "north", "east", "south", "west", "up", "down",
    "northeast", "northwest", "southeast", "southwest",
};

int map_dir_index(const char *dir_name) {
    for (int i = 0; i < MAP_NUM_EXITS; i++) {
        if (strcmp(dir_name, MAP_DIR_NAMES[i]) == 0)
            return i;
    }
    return -1;
}

void map_model_init(map_model_t *m) {
    m->count = 0;
}

map_room_t *map_model_find(map_model_t *m, int vnum) {
    for (int i = 0; i < m->count; i++) {
        if (m->rooms[i].vnum == vnum)
            return &m->rooms[i];
    }
    return NULL;
}

bool map_model_upsert(map_model_t *m, int vnum, const char *name, const int exits[MAP_NUM_EXITS],
                       int x, int y, int z, bool has_pos) {
    map_room_t *r = map_model_find(m, vnum);
    if (!r) {
        if (m->count >= MAP_ROOM_MAX)
            return false;
        r = &m->rooms[m->count++];
        r->vnum = vnum;
    }
    snprintf(r->name, sizeof(r->name), "%s", name ? name : "");
    memcpy(r->exits, exits, sizeof(r->exits));
    r->x = x;
    r->y = y;
    r->z = z;
    r->has_pos = has_pos;
    return true;
}

/* Room names are short DB-sourced text (see gmcp.c's own escaping
 * comment) that should never legitimately contain a tab or newline;
 * strips them defensively anyway so a malformed name can never split
 * or corrupt a saved line. */
static void sanitize_name(const char *src, char *dst, size_t dst_sz) {
    size_t o = 0;
    for (const char *p = src; *p && o + 1 < dst_sz; p++)
        dst[o++] = (*p == '\t' || *p == '\r' || *p == '\n') ? ' ' : *p;
    dst[o] = '\0';
}

void map_model_load(map_model_t *m, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    char line[512];
    while (m->count < MAP_ROOM_MAX && fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if (n == 0)
            continue;
        char *vnum_s = line;
        char *name_s = strchr(vnum_s, '\t');
        if (!name_s)
            continue;
        *name_s++ = '\0';
        char *exits_s = strchr(name_s, '\t');
        if (!exits_s)
            continue;
        *exits_s++ = '\0';
        int vnum = atoi(vnum_s);
        /* A trailing "\tX,Y,Z" field is new (TODO.md real-GDI map view) --
           an older map.dat/world_map.dat saved before it exists has none,
           so this room stays has_pos=false until re-learned/re-exported. */
        char *coords_s = strchr(exits_s, '\t');
        bool has_pos = false;
        int x = 0, y = 0, z = 0;
        if (coords_s) {
            *coords_s++ = '\0';
            if (sscanf(coords_s, "%d,%d,%d", &x, &y, &z) == 3)
                has_pos = true;
        }
        int exits[MAP_NUM_EXITS];
        for (int i = 0; i < MAP_NUM_EXITS; i++)
            exits[i] = -1;
        char *tok = strtok(exits_s, ",");
        for (int i = 0; i < MAP_NUM_EXITS && tok; i++) {
            exits[i] = atoi(tok);
            tok = strtok(NULL, ",");
        }
        map_model_upsert(m, vnum, name_s, exits, x, y, z, has_pos);
    }
    fclose(f);
}

bool map_model_save(const map_model_t *m, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f)
        return false;
    char safe_name[128];
    for (int i = 0; i < m->count; i++) {
        const map_room_t *r = &m->rooms[i];
        sanitize_name(r->name, safe_name, sizeof(safe_name));
        fprintf(f, "%d\t%s\t", r->vnum, safe_name);
        for (int d = 0; d < MAP_NUM_EXITS; d++)
            fprintf(f, "%s%d", d ? "," : "", r->exits[d]);
        if (r->has_pos)
            fprintf(f, "\t%d,%d,%d\n", r->x, r->y, r->z);
        else
            fputc('\n', f);
    }
    fclose(f);
    return true;
}
