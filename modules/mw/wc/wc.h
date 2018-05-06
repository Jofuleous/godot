/* wc.h */
// world creator
#ifndef WC_H
#define WC_H

#include "reference.h"
#include "cmap.h"
#include "chunk.h"

#define CREATE_CHUNK_RADIUS 10
#define RENDER_CHUNK_RADIUS 10
#define RENDER_SIGN_RADIUS 4
#define DELETE_CHUNK_RADIUS 14
#define CHUNK_SIZE 32
#define MAX_CHUNKS 8192
#define MAX_PLAYERS 16
#define WORKERS 4
#define MAX_TEXT_LENGTH 256
#define MAX_NAME_LENGTH 32
#define MAX_PATH_LENGTH 256
#define MAX_ADDR_LENGTH 256

#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2

#define MODE_OFFLINE 0
#define MODE_ONLINE 1

#define WORKER_IDLE 0
#define WORKER_BUSY 1
#define WORKER_DONE 2

#define XZ_SIZE (CHUNK_SIZE * 3 + 2)
#define XZ_LO (CHUNK_SIZE)
#define XZ_HI (CHUNK_SIZE * 2 + 1)
#define Y_SIZE 258
#define XYZ(x, y, z) ((y) * XZ_SIZE * XZ_SIZE + (x) * XZ_SIZE + (z))
#define XZ(x, z) ((x) * XZ_SIZE + (z))

typedef struct {
	int p;
	int q;
	int load;
	CMap *block_maps[3][3];
	CMap *light_maps[3][3];
	int miny;
	int maxy;
	int faces;
	Array mesh_array;
} WorkerItem;

typedef struct {
	int index;
	int state;
	//thrd_t thrd;
	//mtx_t mtx;
	//cnd_t cnd;
	WorkerItem item;
} Worker;

typedef struct {
	int x;
	int y;
	int z;
	int w;
} Block;

typedef struct {
	float x;
	float y;
	float z;
	float rx;
	float ry;
	float t;
} State;

typedef void( *world_func )(int, int, int, int, void *);

class WC : public Node {
	GDCLASS( WC, Node );

protected:
	static void _bind_methods();
	void set_material( const Ref<Material> &p_material );
	Ref<Material> get_material() const;

public:
	void create_initial();
	bool get_created();
	static bool s_created;
	WC();

	void create_world( int p, int q, world_func func, void *arg );
	void compute_chunk( WorkerItem *item );
	Chunk *find_chunk( int p, int q );
	void dirty_chunk( Chunk *chunk );
	void request_chunk( int p, int q );
	void load_chunk( WorkerItem *item );
	void init_chunk( Chunk *chunk, int p, int q );
	void create_chunk( Chunk *chunk, int p, int q );
	void generate_chunk( Chunk *chunk, WorkerItem *item );
	void gen_chunk_buffer( Chunk *chunk );
	int highest_block( float x, float z );
	void delete_chunks();
	void delete_all_chunks();
	void force_chunks( float x, float z );
	void check_workers();
	int worker_run( void *arg );
	void ensure_chunks_worker( float x, float z, Worker *worker );
	void ensure_chunks( float x, float z );
	int get_block( int x, int y, int z );

	Ref<Material> material;
	Node* world_node;
	Worker workers[WORKERS];
	Chunk chunks[MAX_CHUNKS];
	int chunk_count;
	int create_radius;
	int render_radius;
	int delete_radius;
	int sign_radius;
	//Player players[MAX_PLAYERS];
	int player_count;
	int typing;
	char typing_buffer[MAX_TEXT_LENGTH];
	int message_index;
	//char messages[MAX_MESSAGES][MAX_TEXT_LENGTH];
	int width;
	int height;
	int observe1;
	int observe2;
	int flying;
	int item_index;
	int scale;
	int ortho;
	float fov;
	int suppress_char;
	int mode;
	int mode_changed;
	char db_path[MAX_PATH_LENGTH];
	char server_addr[MAX_ADDR_LENGTH];
	int server_port;
	int day_length;
	int time_changed;
	Block block0;
	Block block1;
	Block copy0;
	Block copy1;
};

#endif