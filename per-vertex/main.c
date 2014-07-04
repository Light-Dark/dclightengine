/*
	PVR LIGHTING /w BUMP MAPPING EXAMPLE
	- Uses the PowerVR's bumpmapping and a per-vertex lighting engine I made
	
	By: Liam "Dragmire" Ewasko
	2014
	

*/


#include <kos.h>
#include <math.h>
#include <oggvorbis/sndoggvorbis.h>
#define LIGHTS 2
#define LAYER_SIZE (640/128)*(480/128) + 5
#define MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define MIN(a,b) (((a)<(b))?(a):(b))



typedef unsigned int Uint32;
typedef unsigned char Uint8, uint8;

/*
	First Half of the 1024 will go to 32 4-bit palettes
*/
static Uint32 Pindex4 = 0;
/*
	Second half will go to 2 8-bit palettes
*/
static Uint32 Pindex8 = 0;
/*
	Tvspelsfreak's texture file header
*/

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
    Vector3 c;
    //Vector3 n;
    Vector3 FinalColor;
}Vertex;

typedef struct{
    Vertex verts[4];
    Material mat;
    Vector3 surfacenormal;
}Quad;



typedef struct{
    Vector3 p;
    Vector3 c;
	Vector3 Dir;
    float aa,ab,ac; //Attenuation coefficients
    float cosInnerCone,cosOuterCone;
    Uint8 Spotlight;
	
}Light;




static Vector3 GlobalAmbient = {0.0,0.0,0.0,1.0};
static Vector3 pos3 = {0.0,0.0,1.0,1.0};
Vector3 FinalAmbient = {0,0,0,1.0};
Vector3 L  = {0,0,0,1.0};
Vector3 FinalDiffuse = {0,0,0,1.0};
Vector3 EmissAmb = {0,0,0,1.0};
Vector3 FinalPos = {0,0,0,1.0};
Vector3 FinalSpec = {0,0,0,1.0};
Vector3 pos1 = {0,0,0,1.0};
Vector3 pos2 = {0,0,0,1.0};
Vector3 FinalClr = {0.0,0.0,0.0,1.0};
float dot = 0.0;
float diffuselight = 0.0;
float d = 1.0;
float atten = 0.0;	

Texture GlobalNormal;
Texture GlobalTex;

Quad Layer[LAYER_SIZE];
Light Lights[LIGHTS];



void Load_Texture(const char* fn, Texture* t){
	FILE* fp;
	header_t  hdr;
	fp = fopen(fn,"r");
	
	fread(&hdr,sizeof(hdr),1,fp);	// read in the header
	
	t->w = hdr.width;
	t->h = hdr.height;
	
	t->fmt = hdr.type;
	//Allocate texture memory
	t->txt = pvr_mem_malloc(hdr.size);
	//Temporary ram storage of texture
	void* temp = malloc(hdr.size);
	// Load texture into ram
	fread(temp,hdr.size,1,fp);
	// SQ copy into VRAM
	pvr_txr_load(temp,t->txt,hdr.size);
	//Free RAM
	free(temp);
	temp = NULL;
	fclose(fp);
	
	/*
		Palette loading and management
	*/
	if( ((t->fmt >> 27) & 7) > 4 ) {
		if(t->fmt & PVR_TXRFMT_PAL4BPP){
			// Append palette suffix to filepath
			char pf[64];
			strcpy(pf,fn);
			strcat(pf,".pal");
			fp = fopen(pf,"r");
			pal_header_t phdr;
			//read in the 8-byte header
			fread(&phdr,sizeof(pal_header_t),1,fp);
			//setup buffer
			void *palette = malloc(phdr.numcolors*4);
			Uint32 i;
			//Make entries readable to PVR
			Uint32* packed = (Uint32*)palette;
			//Load entries in to buffer
			fread(packed,phdr.numcolors*4,1,fp);
			//Load palette entries into correct location ( first 512 bank)
			for(i = Pindex4*16; i < (Pindex4*16) + phdr.numcolors*4;i++){
				pvr_set_pal_entry(i,packed[i]);
			}
			//Set palette #
			t->palette = Pindex4;
		
			t->fmt |=  PVR_TXRFMT_4BPP_PAL(Pindex4);
		
		//Increase palettte index to prevent overwrite
			Pindex4++;
			//32 possible palettes in 512 bank
			if(Pindex4 == 32){
				Pindex4 = 0; // overwrite
			}

			packed = NULL;
			free(palette);
			fclose(fp);
		} else if(t->fmt & PVR_TXRFMT_PAL8BPP){
			char pf[64];
			strcpy(pf,fn);
			strcat(pf,".pal");
			fp = fopen(pf,"r");
			pal_header_t phdr;
			fread(&phdr,sizeof(pal_header_t),1,fp);
			void * palette = malloc(phdr.numcolors*4);
			Uint32 i;
			Uint32* packed = (Uint32*)palette;
			fread(packed,phdr.numcolors*4,1,fp);
			
			//Load palette entries into the second 512 bank
			for(i = (512 + Pindex8*256); i < (Pindex8*256 + 512) + phdr.numcolors*4;i++){
				pvr_set_pal_entry(i,packed[i]);
			}
			
			t->palette = Pindex8 | 0x80;
			t->fmt |=  PVR_TXRFMT_8BPP_PAL(Pindex8+2);
			Pindex8++;
			
			//Only 2 8-bit palettes can fit into second 512 bank
			if(Pindex8 == 2){
				Pindex8 = 0;
			}
		
			packed = NULL;
			free(palette);
			fclose(fp);
		}
	}
	
}



inline float smoothstep(float min, float max, float x){
	if(x < min)
		return 0;
	if(x >= max)
		return 1;
	x  = (x-min)/(max-min);
	
	return (-2*(x*x*x)) + \
			(3*(x*x));
}

inline void Normalize(Vector3 *in,Vector3 *out){
	//inverse square root is apparently faster on Dreamcast
	float length = frsqrt(fipr_magnitude_sqr(in->x, in->y, in->z, 0.0));
	out->x = in->x*length;
	out->y = in->y*length;
	out->z = in->z*length;

}

inline void Dot(Vector3 *vec1,Vector3* vec2,float* out){
	*out = fipr(vec1->x, vec1->y, vec1->z, 0.0, vec2->x, vec2->y, vec2->z, 0.0);
}

inline void Mult_Vector3(Vector3* vec1,Vector3* vec2,Vector3 *out){
	out->x = vec1->x*vec2->x;
	out->y = vec1->y*vec2->y;
	out->z = vec1->z*vec2->z;
}

inline void Add_Vector3(Vector3* vec1,Vector3* vec2,Vector3 *out){
	out->x = vec1->x + vec2->x;
	out->y = vec1->y + vec2->y;
	out->z = vec1->z + vec2->z;
}

inline void Cross(Vector3 *v1,Vector3* v2,Vector3 *out){
	out->x = (v1->y*v2->z) - (v1->z*v2->y);
	out->y = (v1->z*v2->x) - (v1->x*v2->z);
	out->z = (v1->x*v2->y) - (v1->y*v2->x);
	out->w = 1.0;
}


inline float DualConeSpotlight(pvr_vertex_t *P,Light* l){
	Vector3 V;
	pos1.x = P->x - l->p.x;
	pos1.y = P->y - l->p.y;
	pos1.z = P->z - l->p.z;
	Normalize(&pos1,&V);
	float cosDirection;
	Dot(&V,&l->Dir,&cosDirection);
	return smoothstep(l->cosOuterCone,l->cosInnerCone,cosDirection);
}




void Light_Vert(pvr_vertex_t *p,Vector3 *n,Light *l,Vector3* outclr,Material *mat){

	Mult_Vector3(&mat->Ambient,&GlobalAmbient,&FinalAmbient);
	FinalPos.x = l->p.x - p->x;
	
	FinalPos.y = l->p.y - p->y;
	FinalPos.z = l->p.z - p->z;

	Normalize(&FinalPos,&L);

	Dot(n,&L,&dot);
	diffuselight = MAX(dot,0.0);

	Mult_Vector3(&l->c,&mat->Diffuse,&FinalDiffuse);
	d = frsqrt(fipr_magnitude_sqr(FinalPos.x,FinalPos.y,FinalPos.z,0.0));
	atten = 1.0 / (l->aa + l->ab * (d + l->ac / (d / d)));

	FinalPos.x = 640/2 - p->x;
	FinalPos.y = 480/2 - p->y;
	FinalPos.z = 256 - p->z;
	Vector3 V;
	Normalize(&FinalPos,&V);
	Add_Vector3(&L,&V,&pos1);

	Vector3 H;
	Normalize(&pos1,&H);
	Dot(n,&H,&dot);

	float specularLight = pow(MAX(dot, 0),mat->shine);
	
	if (diffuselight <= 0) specularLight = 0;
	
	Mult_Vector3(&mat->Specular,&l->c,&FinalSpec);
	if(l->Spotlight == 0) {
		FinalSpec.x *=  specularLight * atten;
		FinalSpec.y *=  specularLight * atten;
		FinalSpec.z *=  specularLight * atten;
	
		FinalDiffuse.x *= diffuselight * atten;
		FinalDiffuse.y *= diffuselight * atten;
		FinalDiffuse.z *= diffuselight * atten;
	} else {
		float spot = DualConeSpotlight(p,l);
		FinalSpec.x *=  specularLight * spot * atten;
		FinalSpec.y *=  specularLight * spot * atten;
		FinalSpec.z *=  specularLight * spot * atten;
	
		FinalDiffuse.x *= diffuselight * spot * atten;
		FinalDiffuse.y *= diffuselight * spot * atten;
		FinalDiffuse.z *= diffuselight * spot * atten;	
	}

	Add_Vector3(&mat->Emissive,&FinalAmbient,&EmissAmb);
	Add_Vector3(&FinalDiffuse,&EmissAmb,&FinalAmbient);
	Add_Vector3(&FinalSpec,&FinalAmbient,&FinalClr);
	//Blend and clamp
	outclr->x += FinalClr.x;
	outclr->x = MIN(outclr->x,1.0);
	outclr->y += FinalClr.y;
	outclr->y = MIN(outclr->y,1.0);
	outclr->z += FinalClr.z;
	outclr->z = MIN(outclr->z,1.0);
	outclr->w = 1.0;
}



void LightQuad(Quad  *qd,Light* l){
	int i;
	//Calculate surface normal
	pos1.x = qd->verts[1].p.x - qd->verts[0].p.x;
	pos1.y = qd->verts[1].p.y - qd->verts[0].p.y;
	pos1.z = qd->verts[1].p.z - qd->verts[0].p.z;
	pos2.x = qd->verts[2].p.x - qd->verts[0].p.x;
	pos2.y = qd->verts[2].p.y - qd->verts[0].p.y;
	pos2.z = qd->verts[2].p.z - qd->verts[0].p.z;
	Cross(&pos1,&pos2,&pos3);
	Normalize(&pos3,&qd->surfacenormal);
    for(i = 0;i< 4;i++){
		Light_Vert(&qd->verts[i].p,&qd->surfacenormal,l,&qd->verts[i].FinalColor,&qd->mat);
    }

}

uint32 getBumpParameters(float T, float Q, float h) {
	unsigned char Q2 = (unsigned char) ((Q / (2 * 3.1415927))* 255);
	unsigned char h2 = (unsigned char) (h * 255);

	unsigned char k1 = 255 - h2;
	unsigned char k2 = (unsigned char) (h2 * fsin(T));
	unsigned char k3 = (unsigned char) (h2 * fcos(T));
	
	int oargb = k1 << 24 | k2 << 16 | k3 << 8 | Q2;

	return oargb;
}


void Draw_Bump(Quad *qd,Light *l){
	int i;
	pvr_poly_cxt_t p_cxt;
	pvr_poly_hdr_t p_hdr;
	
	pvr_poly_cxt_txr(&p_cxt,PVR_LIST_TR_POLY,qd->mat.bumpmap.fmt,qd->mat.bumpmap.w,qd->mat.bumpmap.w,qd->mat.bumpmap.txt,PVR_FILTER_BILINEAR);
	p_cxt.gen.alpha = PVR_ALPHA_ENABLE;
	pvr_poly_compile(&p_hdr,&p_cxt);
	p_hdr.cmd |= 4;

	Vector3 D;
	D.x = (qd->verts[i].p.x+16) - l->p.x;
	D.y = (qd->verts[i].p.y+16) - l->p.y ;
	D.z = 0;
	float T = l->p.z*frsqrt(fipr_magnitude_sqr(D.x,D.y,0.0,0.0));//0.5 * 3.141592/2;
	float Q = ((D.y/D.x)) * (3.141592*2);
	float h = 0.5;
	pvr_prim(&p_hdr,sizeof(pvr_poly_hdr_t));
	Uint32 oargb = getBumpParameters(T,Q,h);
	for(i=0;i<4;i++){
		qd->verts[i].p.oargb = oargb;
		qd->verts[i].p.argb = PVR_PACK_COLOR(1.0,0,0,0);
		pvr_prim(&qd->verts[i].p,sizeof(pvr_vertex_t));

	}
	
}


void Draw_Quad(Quad* qd){
	int i;
	pvr_poly_cxt_t p_cxt;
	pvr_poly_hdr_t p_hdr;

	pvr_poly_cxt_txr(&p_cxt,PVR_LIST_OP_POLY,GlobalTex.fmt,GlobalTex.w,GlobalTex.h,GlobalTex.txt,PVR_FILTER_BILINEAR);
	pvr_poly_compile(&p_hdr,&p_cxt);
	pvr_prim(&p_hdr,sizeof(p_hdr)); // submit header
	for( i =0; i < 4;i++){
		qd->verts[i].p.argb = PVR_PACK_COLOR(0.0,qd->verts[i].FinalColor.x,qd->verts[i].FinalColor.y,qd->verts[i].FinalColor.z);;
		qd->verts[i].p.oargb = 0;

		pvr_prim(&qd->verts[i].p,sizeof(pvr_vertex_t));
		qd->verts[i].FinalColor.x = 0;
		qd->verts[i].FinalColor.y = 0;
		qd->verts[i].FinalColor.z = 0; 
	}
}


void Init_Quad(Quad* qd,float x,float y,float w,float h){
	qd->verts[0].p.x = x;
	qd->verts[0].p.y = y;
	qd->verts[0].p.z = 1.0;
	qd->verts[0].p.flags = PVR_CMD_VERTEX;
	qd->verts[0].p.u = 0.0;
	qd->verts[0].p.v = 0.0;
	qd->verts[0].p.oargb = 0;

	qd->verts[0].c.x = 0.0;
	qd->verts[0].c.y = 0.0;
	qd->verts[0].c.z = 0.0;


	qd->verts[1].p.x = x+w;
	qd->verts[1].p.y = y;
	qd->verts[1].p.z = 1.0;
	qd->verts[1].p.flags = PVR_CMD_VERTEX;
	qd->verts[1].p.u = 1.0;
	qd->verts[1].p.v = 0.0;
	qd->verts[1].p.oargb = 0;

	qd->verts[1].c.x = 0.0;
	qd->verts[1].c.y = 0.0;
	qd->verts[1].c.z = 0.0;

	qd->verts[2].p.x = x;
	qd->verts[2].p.y = y+h;
	qd->verts[2].p.z = 1.0;
	qd->verts[2].p.flags = PVR_CMD_VERTEX;
	qd->verts[2].p.u = 0.0;
	qd->verts[2].p.v = 1.0;
	qd->verts[2].p.oargb = 0;

	qd->verts[2].c.x = 0.0;
	qd->verts[2].c.y = 0.0;
	qd->verts[2].c.z = 0.0;

	qd->verts[3].p.x = x+w;
	qd->verts[3].p.y = y+h;
	qd->verts[3].p.z = 1.0;
	qd->verts[3].p.flags = PVR_CMD_VERTEX_EOL;
	qd->verts[3].p.u = 1.0;
	qd->verts[3].p.v = 1.0;
	qd->verts[3].p.oargb = 0;

	qd->verts[3].c.x = 0.0;
	qd->verts[3].c.y = 0.0;
	qd->verts[3].c.z = 0.0;

	qd->mat.Diffuse.x = 1.0;
	qd->mat.Diffuse.y = 1.0;
	qd->mat.Diffuse.z = 1.0;

	qd->mat.Emissive.x = 0.0;
	qd->mat.Emissive.y = 0.0;
	qd->mat.Emissive.z = 0.0;

	qd->mat.Ambient.x = 0.0;
	qd->mat.Ambient.y = 0.0;
	qd->mat.Ambient.z = 0.0;
	
	qd->mat.Specular.x = 0.0;
	qd->mat.Specular.y = 0.0;
	qd->mat.Specular.z = 0.0;

	qd->surfacenormal.x = 0;
	qd->surfacenormal.y = 0;
	qd->surfacenormal.z = 1.0;
	qd->surfacenormal.w = 1.0;
	
	qd->mat.bumpmapped  = 1.0;
	qd->mat.bumpmap.txt = GlobalNormal.txt;
	qd->mat.bumpmap.w = GlobalNormal.w;
	qd->mat.bumpmap.h = GlobalNormal.h;
	qd->mat.bumpmap.fmt = GlobalNormal.fmt;

	qd->mat.shine = 1.0;

}

void Init_Layer(){
	int i;
	float x = 0;
	float y = 0;
    for(i = 0; i < LAYER_SIZE;i++){
		Init_Quad(&Layer[i],x,y,128,128);
		x += 128.0;
        if(x > 640){
			x = 0.0;
			y+= 128.0;
        }
    }
}
void Draw_Layer(){
	int i;
	int z;
    for(i = 0; i < LAYER_SIZE;i++){
        for(z = 0; z < LIGHTS;z++){
			LightQuad(&Layer[i],&Lights[z]);
        }
		Draw_Quad(&Layer[i]);
    }
}
void Draw_Layer_Bump(){
	int i;
	for(i = 0; i < LAYER_SIZE;i++){
		if(Layer[i].mat.bumpmapped == 1){
			Draw_Bump(&Layer[i],&Lights[0]);
		}
	}
}



void Init(){
	/*
		Standard initialization
	*/
	vid_set_mode(DM_640x480,PM_RGB565);
	vid_border_color(0,255,0);
	pvr_init_params_t params = {
                { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_16 },
                1024*1024,0
        };

	pvr_init(&params);
	//Set palette to ARGB8888 format
	
	//pvr_set_pal_format(PVR_PAL_ARGB8888);
	
	//Initialize ogg streamer
	snd_stream_init();
	sndoggvorbis_init();
	
}



extern uint8 romdisk[];

KOS_INIT_FLAGS(INIT_DEFAULT);
KOS_INIT_ROMDISK(romdisk);

int main(int argc,char **argv){
	Init();
//	sndoggvorbis_start("/rd/billy.ogg",-1);
	Lights[0].p.z = 10.0;
	Lights[0].p.x = 0;
	Lights[0].p.y = 0;
	Lights[0].c.x = 5.0;
	Lights[0].c.y = 5.0;
	Lights[0].c.z = 5.0;
	Lights[0].c.w = 1.0;
	Lights[0].aa = 1.0;
	Lights[0].ab = 0.0;
	Lights[0].ac = 0.0;
	Lights[0].Spotlight = 0;

	Lights[1].p.z = 10.0;
	Lights[1].p.x = 640/2.0;
	Lights[1].p.y = 64.0;
	Lights[1].c.x = 5.0;
	Lights[1].c.y = 0.0;
	Lights[1].c.z = 0.0;
	Lights[1].c.w = 1.0;
	Lights[1].aa = 1.0;
	Lights[1].ab = 0.0;
	Lights[1].ac = 0.0;

	Lights[2].p.z = 10.0;
	Lights[2].p.x = 640.0 * 0.75;
	Lights[2].p.y = 480/2.0;
	Lights[2].c.x = 0.0;
	Lights[2].c.y = 0.0;
	Lights[2].c.z = 5.0;
	Lights[2].c.w = 1.0;
	Lights[2].aa = 1.0;
	Lights[2].ab = 0.0;
	Lights[2].ac = 0.0;

	Load_Texture("/rd/bumpmap.raw",&GlobalNormal);
	Load_Texture("/rd/text.raw",&GlobalTex);
	Init_Layer();
	int q = 0;
	int x = 0;
	int pushed = 0;
	while(q == 0){
		vid_border_color(255,0,0);
		pvr_wait_ready();
		vid_border_color(0,255,0);
		pvr_scene_begin();
			
		pvr_list_begin(PVR_LIST_OP_POLY);
			Draw_Layer();
		pvr_list_finish();
		pvr_list_begin(PVR_LIST_TR_POLY);
			Draw_Layer_Bump();
		pvr_list_finish();
		pvr_list_begin(PVR_LIST_PT_POLY);

		pvr_list_finish();

		pvr_scene_finish();
		vid_border_color(0,0,255);
		
		//Get controller input
		MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, st);
		
			if(st->buttons & CONT_START)
				q = 1;
				
			if(st->buttons & CONT_DPAD_LEFT)
				Lights[x].p.x -= 4;
			if(st->buttons & CONT_DPAD_RIGHT)
				Lights[x].p.x += 4;
			if(st->buttons & CONT_DPAD_UP)
				Lights[x].p.y -= 4;
			if(st->buttons & CONT_DPAD_DOWN)
				Lights[x].p.y += 4;
				
			if(st->buttons & CONT_A && pushed == 0){
				pushed = 1;
				x++;
				if(x == LIGHTS){
					x = 0;
				}
			} 
			
			if(st->buttons & CONT_B && pushed == 0){
				Lights[x].c.x += 1;
				if(Lights[x].c.x > 10.0)
					Lights[x].c.x = 0.0;
				pushed = 1;
			}
			
			if(st->buttons & CONT_X && pushed == 0){
				Lights[x].c.y += 1;
				if(Lights[x].c.y > 10.0)
					Lights[x].c.y = 0.0;
				pushed = 1;
			}
			
			if(st->buttons & CONT_Y && pushed == 0){
				Lights[x].c.z += 1;
				if(Lights[x].c.z > 10.0)
					Lights[x].c.z = 0.0;
				pushed = 1;
			}
			
			if(!(st->buttons & CONT_A) && !(st->buttons & CONT_B) && !(st->buttons & CONT_X) && !(st->buttons & CONT_Y)){
				pushed = 0;
			}
			
			
		
		MAPLE_FOREACH_END();
		
	}

	sndoggvorbis_stop();
	pvr_shutdown();
	sndoggvorbis_shutdown();
	return 0;
}