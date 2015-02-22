#ifndef LIGHT_H
#define LIGHT_H


#define LIGHTS 3
#define MAX_LIGHTS 3
#define LAYER_SIZE ((640/64)*((480/64)+(480/64)))
#define PI 3.14159265f
#define PI_FLOAT     3.14159265f
#define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define MIN(a,b) (((a)<(b))?(a):(b))
/* Some #define's so we can keep the nice looking matrices for reference */
#define XCENTER 0.0f
#define YCENTER 0.0f
#define COT_FOVY_2 1.0f
#define ZNEAR .1f
#define ZFAR 255.0f
#define PIBY2_FLOAT  1.5707963f

typedef unsigned int Uint32;
typedef unsigned char Uint8, uint8;


/*
	Tvspelsfreak's texture file header
*/
typedef struct {
	char	id[4];	// 'DTEX'
	short	width;
	short	height;
	Uint32		type;
	int		size;
} header_t;


typedef struct {
	char	id[4];	// 'DPAL'
	int		numcolors;
} pal_header_t;

/*
	My texture structure
*/
typedef struct {
	uint32 w,h;
	uint32 fmt;
	pvr_ptr_t txt;
	Uint8 palette;
}Texture;


typedef struct {
	float x,y,z,w;
}Vector3 __attribute__((aligned(32)));

typedef struct {
	Vector3 Emissive;
	Vector3 Ambient;
	Vector3 Diffuse;
	Vector3 Specular;
	Texture bumpmap;
	Uint8 bumpmapped;
	float shine;
}Material;

typedef struct{
	pvr_vertex_t p;
	pvr_vertex_t trans;	//transformed vertex
	Vector3 c;
	Vector3 FinalColor;
}Vertex;

typedef struct{
	Vertex verts[4];
	Material mat;
	Vector3 surfacenormal;
}Quad;



typedef struct{
	float x,y,z,w;
	float ac,ab,aa,dummy;
	float r,g,b,a;
}Light __attribute__((aligned(32)));



void _lightvertex(void* vertex,const void* light,void * outclr,void* surfacenormal);
void normalize(void* vert1,void *vertnorm);

#endif
