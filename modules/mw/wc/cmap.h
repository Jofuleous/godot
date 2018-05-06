#ifndef _cmap_h_
#define _cmap_h_

#define EMPTY_ENTRY(entry) ((entry)->value == 0)

#define MAP_FOR_EACH(map, ex, ey, ez, ew) \
    for (unsigned int i = 0; i <= map->mask; i++) { \
        CMapEntry *entry = map->data + i; \
        if (EMPTY_ENTRY(entry)) { \
            continue; \
        } \
        int ex = entry->e.x + map->dx; \
        int ey = entry->e.y + map->dy; \
        int ez = entry->e.z + map->dz; \
        int ew = entry->e.w;

#define END_MAP_FOR_EACH }

typedef union {
	unsigned int value;
	struct {
		unsigned char x;
		unsigned char y;
		unsigned char z;
		char w;
	} e;
} CMapEntry;

typedef struct {
	int dx;
	int dy;
	int dz;
	unsigned int mask;
	unsigned int size;
	CMapEntry *data;
} CMap;

void map_alloc( CMap *map, int dx, int dy, int dz, int mask );
void map_free( CMap *map );
void map_copy( CMap *dst, CMap *src );
void map_grow( CMap *map );
int map_set( CMap *map, int x, int y, int z, int w );
int map_get( CMap *map, int x, int y, int z );

#endif