#ifndef _CHUNK_H_
#define _CHUNK_H_

#include "scene/resources/mesh.h"
#include "scene/3d/mesh_instance.h"

class Chunk //: public Resource
{
public:
	//GDCLASS( Chunk, Resource );
	CMap map;
	//Map lights;
	//SignList signs;
	int p;
	int q;
	int faces;
	//int sign_faces;
	int dirty;
	int miny;
	int maxy;
	MeshInstance* mesh_instance; // all nodes need to be allocated, apparently.
	//GLuint buffer;
	//GLuint sign_buffer;
};

#endif