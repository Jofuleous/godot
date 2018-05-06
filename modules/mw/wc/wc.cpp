/* summator.cpp */

#include "editor_node.h"
#include "wc.h"
#include "item.h"
#include "cube.h"
#include "../deps/noise/noise.h"
#include "os/memory.h"
#include "scene/3d/mesh_instance.h"

bool WC::s_created = false;

void map_set_func( int x, int y, int z, int w, void *arg ) {
	CMap *map = (CMap *) arg;
	map_set( map, x, y, z, w );
}

int chunked( float x ) {
	return floorf( roundf( x ) / CHUNK_SIZE );
}

int chunk_distance( Chunk *chunk, int p, int q ) {
	int dp = ABS( chunk->p - p );
	int dq = ABS( chunk->q - q );
	return MAX( dp, dq );
}

void WC::_bind_methods() {
	ClassDB::bind_method( D_METHOD( "get_created" ), &WC::get_created );
	ClassDB::bind_method( D_METHOD( "set_material", "material" ), &WC::set_material );
	ClassDB::bind_method( D_METHOD( "get_material"), &WC::get_material );

	ADD_GROUP( "Preload Data", "" );
	ADD_PROPERTY( PropertyInfo( Variant::OBJECT, "Material", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial,SpatialMaterial" ),
		"set_material", "get_material" );
}

void WC::set_material( const Ref<Material> &p_material ) {
	material = p_material;

	//if( !pending_request ) {
		// just apply it, else it'll happen when _update is called.

	
	for( int chunk_index = 0; chunk_index < chunk_count; chunk_index++ )
	{
		Chunk* chunk = &chunks[chunk_index];
		chunk->mesh_instance->set_material_override( material );
		_change_notify();
	}
}

Ref<Material> WC::get_material() const
{
	return material;
}

void WC::create_initial() {
	//if( !EditorNode::get_singleton() ) // todo: it would be cool to preview in the editor :(
	{
		if( !s_created )
		{
			print_line( "World Creator Init \n" );

			create_radius = CREATE_CHUNK_RADIUS;
			render_radius = RENDER_CHUNK_RADIUS;
			delete_radius = DELETE_CHUNK_RADIUS;
			sign_radius = RENDER_SIGN_RADIUS;

			// INITIALIZE WORKER THREADS
			for( int i = 0; i < WORKERS; i++ ) {
				Worker *worker = workers + i;
				worker->index = i;
				worker->state = WORKER_IDLE;
				//mtx_init( &worker->mtx, mtx_plain );
				//cnd_init( &worker->cnd );
				//thrd_create( &worker->thrd, worker_run, worker );
			}

			// ho'boy...i just want accurate buffers right now.
			// consider the case of buffers not being generated?
			for( int y = -1; y <= 1; y++ )
			{
				for( int x = -1; x <= 1; x++ )
				{
					Chunk* chunk = chunks + chunk_count++;
					create_chunk( chunk, x, y );
					//gen_chunk_buffer( chunk );
				}
			}

			force_chunks( 0.0f, 0.0f );
		}

		s_created = true;
	}
}

bool WC::get_created() {
	return s_created;
}

WC::WC() {
	create_initial();
}


void WC::create_world( int p, int q, world_func func, void *arg ) {
	int pad = 1;
	for( int dx = -pad; dx < CHUNK_SIZE + pad; dx++ ) {
		for( int dz = -pad; dz < CHUNK_SIZE + pad; dz++ ) {
			int flag = 1;
			if( dx < 0 || dz < 0 || dx >= CHUNK_SIZE || dz >= CHUNK_SIZE ) {
				flag = -1;
			}
			int x = p * CHUNK_SIZE + dx;
			int z = q * CHUNK_SIZE + dz;
			float f = simplex2( x * 0.01, z * 0.01, 4, 0.5, 2 );
			float g = simplex2( -x * 0.01, -z * 0.01, 2, 0.9, 2 );
			int mh = g * 32 + 16;
			int h = f * mh;
			int w = 1;
			int t = 12;
			if( h <= t ) {
				h = t;
				w = 2;
			}
			// sand and grass terrain
			for( int y = 0; y < h; y++ ) {
				func( x, y, z, w * flag, arg );
			}
			/*
			if( w == 1 ) {
				if( SHOW_PLANTS ) {
					// grass
					if( simplex2( -x * 0.1, z * 0.1, 4, 0.8, 2 ) > 0.6 ) {
						func( x, h, z, 17 * flag, arg );
					}
					// flowers
					if( simplex2( x * 0.05, -z * 0.05, 4, 0.8, 2 ) > 0.7 ) {
						int w = 18 + simplex2( x * 0.1, z * 0.1, 4, 0.8, 2 ) * 7;
						func( x, h, z, w * flag, arg );
					}
				}
				// trees
				int ok = SHOW_TREES;
				if( dx - 4 < 0 || dz - 4 < 0 ||
					dx + 4 >= CHUNK_SIZE || dz + 4 >= CHUNK_SIZE )
				{
					ok = 0;
				}
				if( ok && simplex2( x, z, 6, 0.5, 2 ) > 0.84 ) {
					for( int y = h + 3; y < h + 8; y++ ) {
						for( int ox = -3; ox <= 3; ox++ ) {
							for( int oz = -3; oz <= 3; oz++ ) {
								int d = (ox * ox) + (oz * oz) +
									(y - (h + 4)) * (y - (h + 4));
								if( d < 11 ) {
									func( x + ox, y, z + oz, 15, arg );
								}
							}
						}
					}
					for( int y = h; y < h + 7; y++ ) {
						func( x, y, z, 5, arg );
					}
				}
			} */
			// clouds
			/*
			if( SHOW_CLOUDS ) {
				for( int y = 64; y < 72; y++ ) {
					if( simplex3(
						x * 0.01, y * 0.1, z * 0.01, 8, 0.5, 2 ) > 0.75 )
					{
						func( x, y, z, 16 * flag, arg );
					}
				}
			}
			*/
		}
	}
}


void WC::compute_chunk( WorkerItem *item ) {

	uint32_t opaque_size = XZ_SIZE * XZ_SIZE * Y_SIZE * sizeof( char );
	uint32_t highest_size = XZ_SIZE * XZ_SIZE * sizeof( char );

	//char *light = (char *) Memory::alloc_static(XZ_SIZE * XZ_SIZE * Y_SIZE *sizeof(char), true);
	char *opaque = (char *) Memory::alloc_static( opaque_size, true );
	memset( opaque, 0, opaque_size );
	char *highest = (char *) Memory::alloc_static( highest_size, true );
	memset( highest, 0, highest_size );

	int ox = item->p * CHUNK_SIZE - CHUNK_SIZE - 1;
	int oy = -1;
	int oz = item->q * CHUNK_SIZE - CHUNK_SIZE - 1;

	Array* mesh_array = &item->mesh_array;

	PoolVector<Vector3> points;
	PoolVector<Vector3> normals;
	PoolVector<Vector2> uvs;
	//PoolVector<int> indices;
	mesh_array->resize( VS::ARRAY_MAX );
	mesh_array->set( VS::ARRAY_VERTEX, points );
	mesh_array->set( VS::ARRAY_NORMAL, normals );
	mesh_array->set( VS::ARRAY_TEX_UV, uvs );
	//mesh_array->set( VS::ARRAY_INDEX, indices );

	// check for lights
	/*
	int has_light = 0;
	if (SHOW_LIGHTS) {
		for (int a = 0; a < 3; a++) {
			for (int b = 0; b < 3; b++) {
				CMap *map = item->light_maps[a][b];
				if (map && map->size) {
					has_light = 1;
				}
			}
		}
	}
	*/

	// populate opaque array
	for( int a = 0; a < 3; a++ ) {
		for( int b = 0; b < 3; b++ ) {
			CMap *map = item->block_maps[a][b];
			if( !map ) {
				continue;
			}
			MAP_FOR_EACH( map, ex, ey, ez, ew ) {
				int x = ex - ox;
				int y = ey - oy;
				int z = ez - oz;
				int w = ew;
				// TODO: this should be unnecessary
				if( x < 0 || y < 0 || z < 0 ) {
					continue;
				}
				if( x >= XZ_SIZE || y >= Y_SIZE || z >= XZ_SIZE ) {
					continue;
				}
				// END TODO
				opaque[XYZ( x, y, z )] = !is_transparent( w );
				if( opaque[XYZ( x, y, z )] ) {
					highest[XZ( x, z )] = MAX( highest[XZ( x, z )], y );
				}
			} END_MAP_FOR_EACH;
		}
	}

	// flood fill light intensities
	/*
	if (has_light) {
	for (int a = 0; a < 3; a++) {
	for (int b = 0; b < 3; b++) {
	CMap *map = item->light_maps[a][b];
	if (!map) {
	continue;
	}
	MAP_FOR_EACH(map, ex, ey, ez, ew) {
	int x = ex - ox;
	int y = ey - oy;
	int z = ez - oz;
	light_fill(opaque, light, x, y, z, ew, 1);
	} END_MAP_FOR_EACH;
	}
	}
	}
	*/

	CMap *map = item->block_maps[1][1];

	// count exposed faces
	int miny = 256;
	int maxy = 0;
	int faces = 0;
	MAP_FOR_EACH( map, ex, ey, ez, ew ) {
		if( ew <= 0 ) {
			continue;
		}
		int x = ex - ox;
		int y = ey - oy;
		int z = ez - oz;
		int f1 = !opaque[XYZ( x - 1, y, z )]; // left
		int f2 = !opaque[XYZ( x + 1, y, z )]; // right
		int f3 = !opaque[XYZ( x, y + 1, z )]; // top
		int f4 = !opaque[XYZ( x, y - 1, z )] && (ey > 0); // bottom
		int f5 = !opaque[XYZ( x, y, z - 1 )]; // front
		int f6 = !opaque[XYZ( x, y, z + 1 )]; // back
		int total = f1 + f2 + f3 + f4 + f5 + f6;
		if( total == 0 ) {
			continue;
		}
		if( is_plant( ew ) ) {
			total = 4;
		}
		miny = MIN( miny, ey );
		maxy = MAX( maxy, ey );
		faces += total;
	} END_MAP_FOR_EACH;

	// generate geometry
	int block_count = 0;

	int offset = 0;
	MAP_FOR_EACH( map, ex, ey, ez, ew ) {
		if( ew <= 0 ) {
			continue;
		}
		block_count++;
		int x = ex - ox;
		int y = ey - oy;
		int z = ez - oz;
		int f1 = !opaque[XYZ( x - 1, y, z )]; // left
		int f2 = !opaque[XYZ( x + 1, y, z )]; // right
		int f3 = !opaque[XYZ( x, y + 1, z )]; // top
		int f4 = !opaque[XYZ( x, y - 1, z )] && (ey > 0); // bottom
		int f5 = !opaque[XYZ( x, y, z - 1 )]; // back
		int f6 = !opaque[XYZ( x, y, z + 1 )]; // front
		int total = f1 + f2 + f3 + f4 + f5 + f6;
		if( total == 0 ) {
			continue;
		}
		char neighbors[27] = { 0 };
		char lights[27] = { 0 };
		float shades[27] = { 0 };
		int index = 0;
		for( int dx = -1; dx <= 1; dx++ ) {
			for( int dy = -1; dy <= 1; dy++ ) {
				for( int dz = -1; dz <= 1; dz++ ) {
					neighbors[index] = opaque[XYZ( x + dx, y + dy, z + dz )];
					//lights[index] = light[XYZ(x + dx, y + dy, z + dz)];
					shades[index] = 0;
					if( y + dy <= highest[XZ( x + dx, z + dz )] ) {
						for( int oy = 0; oy < 8; oy++ ) {
							if( opaque[XYZ( x + dx, y + dy + oy, z + dz )] ) {
								shades[index] = 1.0 - oy * 0.125;
								break;
							}
						}
					}
					index++;
				}
			}
		}
		float ao[6][4];
		float light[6][4];
		// don't worry about AO for now?
		//occlusion(neighbors, lights, shades, ao, light);
		/*
		if (is_plant(ew)) {
		total = 4;
		float min_ao = 1;
		float max_light = 0;
		for (int a = 0; a < 6; a++) {
		for (int b = 0; b < 4; b++) {
		min_ao = MIN(min_ao, ao[a][b]);
		max_light = MAX(max_light, light[a][b]);
		}
		}
		float rotation = simplex2(ex, ez, 4, 0.5, 2) * 360;
		//make_plant(
		//    data + offset, min_ao, max_light,
		//    ex, ey, ez, 0.5, ew, rotation);
		}
		*/
		//else {
		make_cube(
			mesh_array, ao, light,
			f1, f2, f3, f4, f5, f6,
			ex, ey, ez, 0.5, ew );
		//}
		offset += total * 60;
	} END_MAP_FOR_EACH;

	//print_line( "Block Entry Count: " + itos( block_count ) );

	Memory::free_static( opaque, true );
	//free(light);
	Memory::free_static( highest, true );

	item->miny = miny;
	item->maxy = maxy;
	item->faces = faces;
}

Chunk* WC::find_chunk( int p, int q ) {
	for( int i = 0; i < chunk_count; i++ ) {
		Chunk *chunk = chunks + i;
		if( chunk->p == p && chunk->q == q ) {
			return chunk;
		}
	}
	return 0;
}

void WC::dirty_chunk( Chunk *chunk ) {
	chunk->dirty = 1;
	/*
	if( has_lights( chunk ) ) {
		for( int dp = -1; dp <= 1; dp++ ) {
			for( int dq = -1; dq <= 1; dq++ ) {
				Chunk *other = find_chunk( chunk->p + dp, chunk->q + dq );
				if( other ) {
					other->dirty = 1;
				}
			}
		}
	}
	*/
}

void WC::request_chunk( int p, int q ) {
	//int key = db_get_key(p, q);
	//client_chunk(p, q, key);
}

void WC::load_chunk( WorkerItem *item ) {
	int p = item->p;
	int q = item->q;
	CMap *block_map = item->block_maps[1][1];
	//CMap *light_map = item->light_maps[1][1];
	//print_line("Loading Chunk " + itos(p) + ", " + itos(q) );
	create_world( p, q, map_set_func, block_map );
	//db_load_blocks(block_map, p, q);
	//db_load_lights(light_map, p, q);
}

void WC::init_chunk( Chunk *chunk, int p, int q ) {
	chunk->p = p;
	chunk->q = q;
	chunk->faces = 0;
	chunk->mesh_instance = memnew( MeshInstance );

	//chunk->sign_faces = 0;
	//chunk->sign_buffer = 0;
	dirty_chunk( chunk );
	//SignList *signs = &chunk->signs;
	//sign_list_alloc( signs, 16 );
	//db_load_signs( signs, p, q );
	CMap *block_map = &chunk->map;
	//CMap *light_map = &chunk->lights;
	int dx = p * CHUNK_SIZE - 1;
	int dy = 0;
	int dz = q * CHUNK_SIZE - 1;
	map_alloc( block_map, dx, dy, dz, 0x7fff );
	//map_alloc( light_map, dx, dy, dz, 0xf );
}

void WC::create_chunk( Chunk *chunk, int p, int q ) {
	init_chunk( chunk, p, q );

	WorkerItem _item;
	WorkerItem *item = &_item;
	item->p = chunk->p;
	item->q = chunk->q;
	item->block_maps[1][1] = &chunk->map;
	//item->light_maps[1][1] = &chunk->lights;
	load_chunk( item );

	request_chunk( p, q );
}

void WC::generate_chunk( Chunk *chunk, WorkerItem *item ) {
	chunk->miny = item->miny;
	chunk->maxy = item->maxy;
	chunk->faces = item->faces;
	//gen_sign_buffer( chunk );

	chunk->mesh.add_surface_from_arrays( Mesh::PrimitiveType::PRIMITIVE_TRIANGLES, item->mesh_array );
	chunk->mesh_instance->set_mesh( &chunk->mesh );
	if( !material.is_null() )
	{
		//chunk->mesh_instance.set_surface_material( 0, material );
		chunk->mesh_instance->set_material_override( material );
	}
	add_child( chunk->mesh_instance );
	set_editable_instance( chunk->mesh_instance, false );
}

void WC::gen_chunk_buffer( Chunk *chunk ) {
	WorkerItem _item;
	WorkerItem *item = &_item;
	item->p = chunk->p;
	item->q = chunk->q;
	for( int dp = -1; dp <= 1; dp++ ) {
		for( int dq = -1; dq <= 1; dq++ ) {
			Chunk *other = chunk;
			if( dp || dq ) {
				other = find_chunk( chunk->p + dp, chunk->q + dq );
			}
			if( other ) {
				item->block_maps[dp + 1][dq + 1] = &other->map;
				//item->light_maps[dp + 1][dq + 1] = &other->lights;
			}
			else {
				item->block_maps[dp + 1][dq + 1] = 0;
				//item->light_maps[dp + 1][dq + 1] = 0;
			}
		}
	}
	compute_chunk( item );
	generate_chunk( chunk, item );
	chunk->dirty = 0;
}

int WC::highest_block( float x, float z ) {
	int result = -1;
	int nx = roundf( x );
	int nz = roundf( z );
	int p = chunked( x );
	int q = chunked( z );
	Chunk *chunk = find_chunk( p, q );
	if( chunk ) {
		CMap *map = &chunk->map;
		MAP_FOR_EACH( map, ex, ey, ez, ew ) {
			if( is_obstacle( ew ) && ex == nx && ez == nz ) {
				result = MAX( result, ey );
			}
		} END_MAP_FOR_EACH;
	}
	return result;
}

void WC::delete_chunks() {
	/*
    int count = g->chunk_count;
    State *s1 = &g->players->state;
    State *s2 = &(g->players + g->observe1)->state;
    State *s3 = &(g->players + g->observe2)->state;
    State *states[3] = {s1, s2, s3};
    for (int i = 0; i < count; i++) {
        Chunk *chunk = g->chunks + i;
        int delete = 1;
        for (int j = 0; j < 3; j++) {
            State *s = states[j];
            int p = chunked(s->x);
            int q = chunked(s->z);
            if (chunk_distance(chunk, p, q) < g->delete_radius) {
                delete = 0;
                break;
            }
        }
        if (delete) {
            map_free(&chunk->map);
            map_free(&chunk->lights);
            sign_list_free(&chunk->signs);
            del_buffer(chunk->buffer);
            del_buffer(chunk->sign_buffer);
            Chunk *other = g->chunks + (--count);
            memcpy(chunk, other, sizeof(Chunk));
        }
    }
    g->chunk_count = count;
	*/
}

void WC::delete_all_chunks() {
    for (int i = 0; i < chunk_count; i++) {
        Chunk *chunk = chunks + i;
        map_free(&chunk->map);
        //map_free(&chunk->lights);
        //sign_list_free(&chunk->signs);
        //del_buffer(chunk->buffer);
        //del_buffer(chunk->sign_buffer);
    }
    chunk_count = 0;
}

// does not use threads...
//void force_chunks(Player *player) {
void WC::force_chunks( float x, float z ) {
    //State *s = &player->state;
    int p = chunked(x);
    int q = chunked(z);
    int r = 1;
    for (int dp = -r; dp <= r; dp++) {
        for (int dq = -r; dq <= r; dq++) {
            int a = p + dp;
            int b = q + dq;
            Chunk *chunk = find_chunk(a, b);
            if (chunk) {
                if (chunk->dirty) {
                    gen_chunk_buffer(chunk);
                }
            }
            else if (chunk_count < MAX_CHUNKS) {
                chunk = chunks + chunk_count++;
                create_chunk(chunk, a, b);
                gen_chunk_buffer(chunk);
            }
        }
    }
}

void WC::check_workers() {
	for( int i = 0; i < WORKERS; i++ ) {
		Worker *worker = workers + i;
		//mtx_lock( &worker->mtx );
		if( worker->state == WORKER_DONE ) {
			WorkerItem *item = &worker->item;
			Chunk *chunk = find_chunk( item->p, item->q );
			if( chunk ) {
				if( item->load ) {
					CMap *block_map = item->block_maps[1][1];
					CMap *light_map = item->light_maps[1][1];
					map_free( &chunk->map );
					//map_free( &chunk->lights );
					map_copy( &chunk->map, block_map );
					//map_copy( &chunk->lights, light_map );
					request_chunk( item->p, item->q );
				}
				generate_chunk( chunk, item );
			}
			for( int a = 0; a < 3; a++ ) {
				for( int b = 0; b < 3; b++ ) {
					CMap *block_map = item->block_maps[a][b];
					CMap *light_map = item->light_maps[a][b];
					if( block_map ) {
						map_free( block_map );
						free( block_map );
					}
					if( light_map ) {
						map_free( light_map );
						free( light_map );
					}
				}
			}
			worker->state = WORKER_IDLE;
		}
		//mtx_unlock( &worker->mtx );
	}
}

int WC::worker_run( void *arg ) {
	Worker *worker = (Worker *) arg;
	int running = 1;
	while( running ) {
		//mtx_lock( &worker->mtx );
		while( worker->state != WORKER_BUSY ) {
			//cnd_wait( &worker->cnd, &worker->mtx );
		}
		//mtx_unlock( &worker->mtx );
		WorkerItem *item = &worker->item;
		if( item->load ) {
			load_chunk( item );
		}
		compute_chunk( item );
		//mtx_lock( &worker->mtx );
		worker->state = WORKER_DONE;
		//mtx_unlock( &worker->mtx );
	}
	return 0;
}

// was player
void WC::ensure_chunks_worker(float x, float z, Worker *worker) {
    //State *s = &player->state;
    //float matrix[16];
   // set_matrix_3d(
   //     matrix, g->width, g->height,
    //    s->x, s->y, s->z, s->rx, s->ry, g->fov, g->ortho, g->render_radius);
   // float planes[6][4];
   // frustum_planes(planes, g->render_radius, matrix);
   // int p = chunked(s->x);
    //int q = chunked(s->z);
	int p = chunked(x);
	int q = chunked(z);
    int r = create_radius;
    int start = 0x0fffffff;
    int best_score = start;
    int best_a = 0;
    int best_b = 0;
    for (int dp = -r; dp <= r; dp++) {
        for (int dq = -r; dq <= r; dq++) {
            int a = p + dp;
            int b = q + dq;
            int index = (ABS(a) ^ ABS(b)) % WORKERS;
            if (index != worker->index) {
                continue;
            }
            Chunk *chunk = find_chunk(a, b);
            if (chunk && !chunk->dirty) {
                continue;
            }
            int distance = MAX(ABS(dp), ABS(dq));
            //int invisible = !chunk_visible(planes, a, b, 0, 256);
            int priority = 0;
            //if (chunk) {
            //    priority = chunk->buffer && chunk->dirty;
            //}
            int score = /*(invisible << 24) | */ (priority << 16) | distance;
            if (score < best_score) {
                best_score = score;
                best_a = a;
                best_b = b;
            }
        }
    }
    if (best_score == start) {
        return;
    }
    int a = best_a;
    int b = best_b;
    int load = 0;
    Chunk *chunk = find_chunk(a, b);
    if (!chunk) {
        load = 1;
        if (chunk_count < MAX_CHUNKS) {
            chunk = chunks + chunk_count++;
            init_chunk(chunk, a, b);
        }
        else {
            return;
        }
    }
    WorkerItem *item = &worker->item;
    item->p = chunk->p;
    item->q = chunk->q;
    item->load = load;
    for (int dp = -1; dp <= 1; dp++) {
        for (int dq = -1; dq <= 1; dq++) {
            Chunk *other = chunk;
            if (dp || dq) {
                other = find_chunk(chunk->p + dp, chunk->q + dq);
            }
            if (other) {
                CMap *block_map = (CMap*) Memory::alloc_static(sizeof(CMap), true );
                map_copy(block_map, &other->map);
                //CMap *light_map = malloc(sizeof(CMap));
                //map_copy(light_map, &other->lights);
                item->block_maps[dp + 1][dq + 1] = block_map;
                //item->light_maps[dp + 1][dq + 1] = light_map;
            }
            else {
                item->block_maps[dp + 1][dq + 1] = 0;
                //item->light_maps[dp + 1][dq + 1] = 0;
            }
        }
    }
    chunk->dirty = 0;
    worker->state = WORKER_BUSY;
    //cnd_signal(&worker->cnd);
}


//void ensure_chunks(Player *player) {
void WC::ensure_chunks( float x, float z ) {
    check_workers();
    //force_chunks(player);
	force_chunks( x, z );
    for (int i = 0; i < WORKERS; i++) {
        Worker *worker = workers + i;
        //mtx_lock(&worker->mtx);
        if (worker->state == WORKER_IDLE) {
            //ensure_chunks_worker(player, worker);
			ensure_chunks_worker( x, z, worker );
        }
        //mtx_unlock(&worker->mtx);
    }
}

/*
void _set_block( int p, int q, int x, int y, int z, int w, int dirty ) {
	Chunk *chunk = find_chunk( p, q );
	if( chunk ) {
		CMap *map = &chunk->map;
		if( map_set( map, x, y, z, w ) ) {
			if( dirty ) {
				dirty_chunk( chunk );
			}
			db_insert_block( p, q, x, y, z, w );
		}
	}
	else {
		db_insert_block( p, q, x, y, z, w );
	}
	if( w == 0 && chunked( x ) == p && chunked( z ) == q ) {
		unset_sign( x, y, z );
		set_light( p, q, x, y, z, 0 );
	}
}

void set_block( int x, int y, int z, int w ) {
	int p = chunked( x );
	int q = chunked( z );
	_set_block( p, q, x, y, z, w, 1 );
	for( int dx = -1; dx <= 1; dx++ ) {
		for( int dz = -1; dz <= 1; dz++ ) {
			if( dx == 0 && dz == 0 ) {
				continue;
			}
			if( dx && chunked( x + dx ) == p ) {
				continue;
			}
			if( dz && chunked( z + dz ) == q ) {
				continue;
			}
			_set_block( p + dx, q + dz, x, y, z, -w, 1 );
		}
	}
	client_block( x, y, z, w );
}

void record_block( int x, int y, int z, int w ) {
	memcpy( &g->block1, &g->block0, sizeof( Block ) );
	g->block0.x = x;
	g->block0.y = y;
	g->block0.z = z;
	g->block0.w = w;
}
*/

int WC::get_block( int x, int y, int z ) {
	int p = chunked( x );
	int q = chunked( z );
	Chunk *chunk = find_chunk( p, q );
	if( chunk ) {
		CMap *map = &chunk->map;
		return map_get( map, x, y, z );
	}
	return 0;
}

/*
void builder_block( int x, int y, int z, int w ) {
	if( y <= 0 || y >= 256 ) {
		return;
	}
	if( is_destructable( get_block( x, y, z ) ) ) {
		set_block( x, y, z, 0 );
	}
	if( w ) {
		set_block( x, y, z, w );
	}
}
*/