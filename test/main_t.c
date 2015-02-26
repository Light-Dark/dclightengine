/*
	PVR LIGHTING /w BUMP MAPPING EXAMPLE
	- Uses the PowerVR's bumpmapping and a per-vertex lighting engine I made
	
	By: Liam "Dragmire" Ewasko
	2014
	

*/


#include <kos.h>
#include <math.h>
#include <oggvorbis/sndoggvorbis.h>
#include "light_t.h"

#define PREFETCH(addr) __asm__ __volatile__("pref @%0" : : "r" (addr))

#define GLOBAL_VBUFSIZE 1024 * 512
#define OP_POLY_VBUFSIZE 512 * 1024
#define OP_MOD_VBUFSIZE 256 * 1024
#define TR_POLY_VBUFSIZE 512 * 1024
#define TR_MOD_VBUFSIZE 256 * 1024
#define PT_POLY_VBUFSIZE 512 * 1024

static Uint8 op_poly_buf[OP_POLY_VBUFSIZE]__attribute__((aligned(32)));
//static Uint8 op_mod_buf[OP_MOD_VBUFSIZE]__attribute__((aligned(32)));
static Uint8 tr_poly_buf[TR_POLY_VBUFSIZE]__attribute__((aligned(32)));
//static Uint8 tr_mod_buf[TR_MOD_VBUFSIZE]__attribute__((aligned(32)));

//#define print
// |error| < 0.005


/*
	First Half of the 1024 will go to 32 4-bit palettes
*/
static Uint32 Pindex4 = 0;
/*
	Second half will go to 2 8-bit palettes
*/
static Uint32 Pindex8 = 0;

Vector3 pos3 = {0.0,0.0,1.0,1.0};
Vector3 pos1 = {0,0,0,1.0};
Vector3 pos2 = {0,0,0,1.0};
int LIGHTS = 3;

/* Frustum matrix (does perspective) */
static matrix_t fr_mat = {
    { COT_FOVY_2,       0.0f,                      0.0f,  0.0f },
    {       0.0f, COT_FOVY_2,                      0.0f,  0.0f },
    {       0.0f,       0.0f, -(ZFAR + ZNEAR) / (ZNEAR - ZFAR), -1.0f },
    {       0.0f,       0.0f, 2 * ZFAR*ZNEAR / (ZNEAR - ZFAR),  1.0f }
};

Texture GlobalNormal;
Texture GlobalTex;

Quad Layer[LAYER_SIZE];
Light Lights[MAX_LIGHTS];

pvr_poly_cxt_t p_cxt;
pvr_poly_hdr_t p_hdr;

float fast_atan2f( float y, float x )
{
	if ( x == 0.0f )
	{
		if ( y > 0.0f ) return PIBY2_FLOAT;
		if ( y == 0.0f ) return 0.0f;
		return -PIBY2_FLOAT;
	}
	float atan;
	float z = y/x;
	if ( fabsf( z ) < 1.0f )
	{
		atan = z/(1.0f + 0.28f*z*z);
		if ( x < 0.0f )
		{
			if ( y < 0.0f ) return atan - PI_FLOAT;
			return atan + PI_FLOAT;
		}
	}
	else
	{
		atan = PIBY2_FLOAT - z/(z*z + 0.28f);
		if ( y < 0.0f ) return atan - PI_FLOAT;
	}
	return atan;
}



void DeleteTexture(Texture* t){
	t->fmt = 0;
	pvr_mem_free(t->txt);
}


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

static pvr_list_t _current_list = PVR_LIST_OP_POLY;
static uint32* _start_ptr __attribute__((aligned(32))) = NULL;
static uint32* _current_ptr __attribute__((aligned(32))) = NULL;

void BeginPrimitives( pvr_list_t list )
{
	_current_list = list;
	uint32* ptr __attribute__((aligned(32)))  = (uint32*)pvr_vertbuf_tail( _current_list );
	QACR0 = ((((uint32)ptr) >> 26) << 2) & 0x1c;
	QACR1 = ((((uint32)ptr) >> 26) << 2) & 0x1c;
	_start_ptr = _current_ptr = (uint32 *) (0xe0000000 | (((uint32)ptr) & 0x03ffffe0));
}

void EndPrimitives( void )
{
	uint32 bytes __attribute__((aligned(32))) = (uint32)_current_ptr - (uint32)_start_ptr;
	if ( bytes != 0 )
		pvr_vertbuf_written( _current_list, bytes );
}

void SendPolyHeader(const pvr_poly_hdr_t *header){
 
    register uint32 *ptr __attribute__((aligned(32))) = (uint32*)( _current_ptr + 8 );
 
    *--ptr = header->d4;        //Written 04 Bytes
    *--ptr = header->d3;        //Written 08 Bytes
    *--ptr = header->d2;        //Written 12 Bytes
    *--ptr = header->d1;        //Written 16 Bytes
    *--ptr = header->mode3;     //Written 20 Bytes
    *--ptr = header->mode2;     //Written 24 Bytes
    *--ptr = header->mode1;     //Written 28 Bytes
    *--ptr = header->cmd;       //Written 32 Bytes
 
    PREFETCH((void *)ptr); //Prefetch
 
    _current_ptr = (uint32*)( ptr + 8 );  //Point to next Position in Vertex Buffer
}

void SendVertex03( const Vertex* vert)
{
	register uint32* _ptr __attribute__((aligned(32))) = _current_ptr;
	_ptr += 8;
	*--_ptr= vert->oargb;
	*--_ptr= PVR_PACK_COLOR(1.0f,vert->FinalColor.x,vert->FinalColor.y,vert->FinalColor.z);
	*(float*)--_ptr= vert->uv.y;
	*(float*)--_ptr= vert->uv.x;
	*(float*)--_ptr= vert->trans.z;
	*(float*)--_ptr= vert->trans.y;
	*(float*)--_ptr= vert->trans.x;
	*--_ptr= vert->cmd;
	PREFETCH((void *)_ptr);
	_ptr += 8;
	_current_ptr = _ptr;
}

/*
	calculates the cross product of 2 vectors
*/
inline void Cross(Vector3 *v1,Vector3* v2,Vector3 *out){
	out->x = (v1->y*v2->z) - (v1->z*v2->y);
	out->y = (v1->z*v2->x) - (v1->x*v2->z);
	out->z = (v1->x*v2->y) - (v1->y*v2->x);
	out->w = 1.0;
}


inline void LightQuad(Quad *qd,Light* l){
	int i;

	pos1.x = qd->verts[1].p.x - qd->verts[0].p.x;
	pos1.y = qd->verts[1].p.y - qd->verts[0].p.y;
	pos1.z = qd->verts[1].p.z - qd->verts[0].p.z;
	pos2.x = qd->verts[2].p.x - qd->verts[0].p.x;
	pos2.y = qd->verts[2].p.y - qd->verts[0].p.y;
	pos2.z = qd->verts[2].p.z - qd->verts[0].p.z;
	Cross(&pos1,&pos2,&pos3);
	normalize(&pos3,&qd->surfacenormal);
	if(qd->verts[0].trans.x >= 0.0f || qd->verts[0].trans.x <= 640.0f || qd->verts[0].trans.y >= 0.0f \
		|| qd->verts[0].trans.y <= 480.0f)
		_lightvertex(&qd->verts[0].trans,l,&qd->verts[0].FinalColor,&qd->surfacenormal);
	if(qd->verts[1].trans.x >= 0.0f || qd->verts[1].trans.x <= 640.0f || qd->verts[1].trans.y >= 0.0f \
		|| qd->verts[1].trans.y <= 480.0f)		
	_lightvertex(&qd->verts[1].trans,l,&qd->verts[1].FinalColor,&qd->surfacenormal);
	if(qd->verts[2].trans.x >= 0.0f || qd->verts[2].trans.x <= 640.0f || qd->verts[2].trans.y >= 0.0f \
		|| qd->verts[2].trans.y <= 480.0f)
	_lightvertex(&qd->verts[2].trans,l,&qd->verts[2].FinalColor,&qd->surfacenormal);
	if(qd->verts[3].trans.x >= 0.0f || qd->verts[3].trans.x <= 640.0f || qd->verts[3].trans.y >= 0.0f \
		|| qd->verts[3].trans.y <= 480.0f)
	_lightvertex(&qd->verts[3].trans,l,&qd->verts[3].FinalColor,&qd->surfacenormal);
}

void Draw_Bump(Quad *qd){
	int i;
	pvr_poly_cxt_txr(&p_cxt,PVR_LIST_TR_POLY,qd->mat.bumpmap.fmt,qd->mat.bumpmap.w,qd->mat.bumpmap.w,qd->mat.bumpmap.txt,PVR_FILTER_BILINEAR);
	p_cxt.gen.specular = PVR_SPECULAR_ENABLE;
	pvr_poly_compile(&p_hdr,&p_cxt);
	
	//p_hdr.cmd |= 4;
	
	/*
		Average out the light source positions
	*/
	static Vector3 D;
	static Vector3 G;
	if(LIGHTS > 1){
		G.x =0;
		G.y = 0;
		G.z = 0;
		for(i = 0; i < LIGHTS;i++){
			G.x += Lights[i].x;
			G.y += Lights[i].y;
			G.z += Lights[i].z;
		}
		G.x /= LIGHTS;
		G.y /= LIGHTS;
		G.z /= LIGHTS;
		D.x = (qd->verts[0].p.x+16) - G.x;
		D.y = (qd->verts[0].p.y+16) - G.y;
		D.z = (qd->verts[0].p.z) - G.z;
	}else{
		D.x = (qd->verts[0].p.x+16) - Lights[0].x;
		D.y = (qd->verts[0].p.y+16) - Lights[0].y;
		D.z = (qd->verts[0].p.z) - Lights[0].z;
	}
	/*
		Calculate Spherical elevation and rotation angles
	*/
	float T = (frsqrt(fipr_magnitude_sqr(D.x,D.y,D.z,0.0)))*PI2;

	float Q = (fast_atan2f(D.y,D.x));
	//pvr_prim(&p_hdr,sizeof(pvr_poly_hdr_t));
	SendPolyHeader(&p_hdr);
	/*
		Pack bump paramters, 1.0 is the "bumpiness"
	*/
	Uint32 oargb = pvr_pack_bump(1.0,T,Q);
	qd->verts[0].FinalColor.x = 0;
	qd->verts[0].FinalColor.y = 0;
	qd->verts[0].FinalColor.z = 0;
	qd->verts[0].oargb = oargb;
	SendVertex03(&qd->verts[0]);
	//pvr_prim(&qd->verts[0].trans,sizeof(pvr_vertex_t));
	
	qd->verts[1].oargb = oargb;
	qd->verts[1].FinalColor.x = 0;
	qd->verts[1].FinalColor.y = 0;
	qd->verts[1].FinalColor.z = 0;
	SendVertex03(&qd->verts[1]);
	
	qd->verts[2].oargb = oargb;
	qd->verts[2].FinalColor.x = 0;
	qd->verts[2].FinalColor.y = 0;
	qd->verts[2].FinalColor.z = 0;
	SendVertex03(&qd->verts[2]);
	
	qd->verts[3].oargb = oargb;
	qd->verts[3].FinalColor.x = 0;
	qd->verts[3].FinalColor.y = 0;
	qd->verts[3].FinalColor.z = 0;
	SendVertex03(&qd->verts[3]);

}

inline void Transform_Quad(Quad* qd){

	mat_trans_single3_nodiv_nomod(qd->verts[0].p.x,qd->verts[0].p.y,qd->verts[0].p.z, \
								qd->verts[0].trans.x,qd->verts[0].trans.y,qd->verts[0].trans.z);	
	
	mat_trans_single3_nodiv_nomod(qd->verts[1].p.x,qd->verts[1].p.y,qd->verts[1].p.z, \
								qd->verts[1].trans.x,qd->verts[1].trans.y,qd->verts[1].trans.z);	
	
	mat_trans_single3_nodiv_nomod(qd->verts[2].p.x,qd->verts[2].p.y,qd->verts[2].p.z, \
								qd->verts[2].trans.x,qd->verts[2].trans.y,qd->verts[2].trans.z);	
	
	mat_trans_single3_nodiv_nomod(qd->verts[3].p.x,qd->verts[3].p.y,qd->verts[3].p.z, \
								qd->verts[3].trans.x,qd->verts[3].trans.y,qd->verts[3].trans.z);	
}

float w = 1.0;
inline void Draw_Quad(Quad* qd){
	//qd->verts[0].trans.argb = PVR_PACK_COLOR(0.0,qd->verts[0].FinalColor.x,qd->verts[0].FinalColor.y,qd->verts[0].FinalColor.z);
	//pvr_prim(&qd->verts[0].trans,sizeof(pvr_vertex_t));
	SendVertex03(&qd->verts[0]);
	qd->verts[0].FinalColor.x = 0;
	qd->verts[0].FinalColor.y = 0;
	qd->verts[0].FinalColor.z = 0; 
		
	SendVertex03(&qd->verts[1]);
	qd->verts[1].FinalColor.x = 0;
	qd->verts[1].FinalColor.y = 0;
	qd->verts[1].FinalColor.z = 0; 
		

	SendVertex03(&qd->verts[2]);
	qd->verts[2].FinalColor.x = 0;
	qd->verts[2].FinalColor.y = 0;
	qd->verts[2].FinalColor.z = 0; 
		
	SendVertex03(&qd->verts[3]);
	qd->verts[3].FinalColor.x = 0;
	qd->verts[3].FinalColor.y = 0;
	qd->verts[3].FinalColor.z = 0; 
	
}

void Draw_Layer(){
	int i;
	int z;
	/*i = LAYER_SIZE;
	while(i--){
		
	}*/
	
	i = LAYER_SIZE;
	while(i--){
		PREFETCH(&Layer[i]);
		Transform_Quad(&Layer[i]);
		z = LIGHTS;
		//if(Layer[i].trans.x >= 0 && Layer[i].trans.y >= 0 && Layer[i].trans.x <= 640 && Layer[i].trans.y <= 480){
		while(z--){
			LightQuad(&Layer[i],&Lights[z]);
		}
		
	}
	i = LAYER_SIZE;
	BeginPrimitives(PVR_LIST_OP_POLY);
	pvr_poly_cxt_txr(&p_cxt,PVR_LIST_OP_POLY,GlobalTex.fmt,GlobalTex.w,GlobalTex.h,GlobalTex.txt,PVR_FILTER_BILINEAR);
	p_cxt.gen.shading = PVR_SHADE_GOURAUD;
	pvr_poly_compile(&p_hdr,&p_cxt);
	SendPolyHeader(&p_hdr);
	while(i--){
		//pvr_prim(&p_hdr,sizeof(p_hdr));
		Draw_Quad(&Layer[i]);
	}
	EndPrimitives();
}

void Init_Quad(Quad* qd,float x,float y,float z,float w,float h){
	qd->verts[0].p.x = x;
	qd->verts[0].p.y = y;
	qd->verts[0].p.z = z;
	qd->verts[0].cmd = PVR_CMD_VERTEX;
	qd->verts[0].uv.x = 0.0;
	qd->verts[0].uv.y = 0.0;
	qd->verts[0].oargb = 0;
	
	memset(&qd->verts[0].trans,0,sizeof(qd->verts[0].trans));

	qd->verts[0].c.x = 0.0;
	qd->verts[0].c.y = 0.0;
	qd->verts[0].c.z = 0.0;


	qd->verts[1].p.x = x+w;
	qd->verts[1].p.y = y;
	qd->verts[1].p.z = z;
	qd->verts[1].cmd = PVR_CMD_VERTEX;
	qd->verts[1].uv.x = 1.0;
	qd->verts[1].uv.y = 0.0;
	qd->verts[1].oargb = 0;
	
	memset(&qd->verts[1].trans,0,sizeof(qd->verts[1].trans));

	qd->verts[1].c.x = 0.0;
	qd->verts[1].c.y = 0.0;
	qd->verts[1].c.z = 0.0;

	qd->verts[2].p.x = x;
	qd->verts[2].p.y = y+h;
	qd->verts[2].p.z = z;
	qd->verts[2].cmd = PVR_CMD_VERTEX;
	qd->verts[2].uv.x = 0.0;
	qd->verts[2].uv.y = 1.0;
	qd->verts[2].oargb = 0;
	
	memset(&qd->verts[2].trans,0,sizeof(qd->verts[2].trans));

	qd->verts[2].c.x = 0.0;
	qd->verts[2].c.y = 0.0;
	qd->verts[2].c.z = 0.0;

	qd->verts[3].p.x = x+w;
	qd->verts[3].p.y = y+h;
	qd->verts[3].p.z = z;
	qd->verts[3].cmd = PVR_CMD_VERTEX_EOL;
	qd->verts[3].uv.x = 1.0;
	qd->verts[3].uv.y = 1.0;
	qd->verts[3].oargb = 0;
	
	memset(&qd->verts[3].trans,0,sizeof(qd->verts[3].trans));

	qd->verts[3].c.x = 0.0;
	qd->verts[3].c.y = 0.0;
	qd->verts[3].c.z = 0.0;
	
	
	//Calculate surface normal


	qd->mat.Diffuse.x = 0.0;
	qd->mat.Diffuse.y = 0.0;
	qd->mat.Diffuse.z = 0.0;

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
	float z = 1.0;
	for(i = 0; i < LAYER_SIZE;i++){
		Init_Quad(&Layer[i],x,y,z,TILE,TILE);
		x += TILE;
		if(x == 640){
			x = 0.0;
			y += TILE;
		}
	}
}

void Draw_Layer_Bump(){
	int i = LAYER_SIZE;
	BeginPrimitives(PVR_LIST_TR_POLY);
	while(i--){
		if(Layer[i].mat.bumpmapped == 1){
			Draw_Bump(&Layer[i]);
		}
	}
	EndPrimitives();
}



void Init(){
	/*
		Standard initialization
	*/
	vid_set_mode(DM_640x480,PM_RGB565);
	vid_border_color(0,255,0);

	pvr_init_params_t pvr_params;
	pvr_params.vertex_buf_size= GLOBAL_VBUFSIZE;
	pvr_params.dma_enabled= 1;
	pvr_params.fsaa_enabled= 0;
	pvr_params.autosort_disabled = 0;
	pvr_params.opb_sizes[PVR_LIST_OP_POLY]= PVR_BINSIZE_32;
	pvr_params.opb_sizes[PVR_LIST_OP_MOD]= PVR_BINSIZE_0;
	pvr_params.opb_sizes[PVR_LIST_TR_POLY]= PVR_BINSIZE_16;
	pvr_params.opb_sizes[PVR_LIST_TR_MOD]= PVR_BINSIZE_0;
	pvr_params.opb_sizes[PVR_LIST_PT_POLY]= PVR_BINSIZE_0;
	
	pvr_init(&pvr_params);

	pvr_set_vertbuf( PVR_LIST_OP_POLY, op_poly_buf, OP_POLY_VBUFSIZE );
	//pvr_set_vertbuf( PVR_LIST_OP_MOD,  op_mod_buf,  OP_MOD_VBUFSIZE  );
	pvr_set_vertbuf( PVR_LIST_TR_POLY, tr_poly_buf, TR_POLY_VBUFSIZE );
	//pvr_set_vertbuf( PVR_LIST_TR_MOD,  tr_mod_buf,  TR_MOD_VBUFSIZE  );
	//pvr_set_vertbuf( PVR_LIST_PT_POLY, pt_poly_buf, PT_POLY_VBUFSIZE );
	
	//Set palette to ARGB8888 format
	pvr_set_pal_format(PVR_PAL_ARGB8888);
	
	mat_identity();

	
	//Initialize ogg streamer
	//snd_stream_init();
	//sndoggvorbis_init();
	
}

float avgfps = -1;
char buf[64];
void running_stats(){
	pvr_stats_t stats;
	pvr_get_stats(&stats);

	avgfps = stats.frame_rate;
			
}

extern uint8 romdisk[];

KOS_INIT_FLAGS(INIT_DEFAULT);
KOS_INIT_ROMDISK(romdisk);

int main(int argc,char **argv){
	Init();
	//sndoggvorbis_start("/pc/billy.ogg",-1);
	Lights[0].z = 10.0;
	Lights[0].x = 0.0;
	Lights[0].y = 0.0;
	Lights[0].w = 1.0;
	Lights[0].r = 5.0;
	Lights[0].g = 0.0;
	Lights[0].b = 0.0;
	Lights[0].a = 1.0;
	Lights[0].aa = 0.0;
	Lights[0].ab = 0.0;
	Lights[0].ac = 1.0;
	Lights[0].dummy = 1.0;
	
	Lights[1].z = 10.0;
	Lights[1].x = 100.0;
	Lights[1].y = 100.0;
	Lights[1].w = 1.0;
	Lights[1].r = 0.0;
	Lights[1].g = 5.0;
	Lights[1].b = 0.0;
	Lights[1].a = 1.0;
	Lights[1].aa = 0.0;
	Lights[1].ab = 0.0;
	Lights[1].ac = 1.0;
	Lights[1].dummy = 1.0;
	
	Lights[2].z = 10.0;
	Lights[2].x = 400.0;
	Lights[2].y = 400.0;
	Lights[2].w = 1.0;
	Lights[2].r = 0.0;
	Lights[2].g = 0.0;
	Lights[2].b = 5.0;
	Lights[2].a = 1.0;
	Lights[2].aa = 0.0;
	Lights[2].ab = 0.0;
	Lights[2].ac = 1.0;
	Lights[2].dummy = 1.0;

	
	vid_border_color(255,0,0);
	Load_Texture("/rd/bumpmap.raw",&GlobalNormal);
	Load_Texture("/rd/text.raw",&GlobalTex);
	vid_border_color(0,0,255);
	Init_Layer();
	
	
	
	int q = 0;
	int x = 0;
	int pushed = 0;
	int bumpenabled = 1;
	int display_fps = 0;
	bfont_set_encoding(BFONT_CODE_ISO8859_1);
	while(q == 0){
		mat_identity();

		
		vid_border_color(255,0,0);
		pvr_wait_ready();
		vid_border_color(0,255,0);
		pvr_scene_begin();
		Draw_Layer();
		if(bumpenabled)
			Draw_Layer_Bump();
	/*	pvr_list_begin(PVR_LIST_OP_POLY);
			Draw_Layer();
		pvr_list_finish();
		
		pvr_list_begin(PVR_LIST_TR_POLY);
		if(bumpenabled)
			Draw_Layer_Bump();
		pvr_list_finish();
		*/
		pvr_scene_finish();
		vid_border_color(0,0,255);
	
		MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, st);
			if(st->buttons & CONT_START)
				q = 1;
			
			if(st->joyx > 32){
				Lights[x].x += 4.0f;
			}
			if(st->joyx < -32){
				Lights[x].x -= 4.0f;
			}
			if(st->joyy < -32){
				Lights[x].y -= 4.0f;
			}
			if(st->joyy > 32){
				Lights[x].y += 4.0f;
			}
			
			
			if(st->buttons & CONT_DPAD_LEFT){
				Lights[x].x -= 4.0f;
			}
			if(st->buttons & CONT_DPAD_RIGHT){
				Lights[x].x += 4.0f;
			}
			if(st->buttons & CONT_DPAD_UP){
				Lights[x].y -= 4;
			}
			if(st->buttons & CONT_DPAD_DOWN){
				Lights[x].y += 4;
			}
				
			if(st->buttons & CONT_A && pushed == 0){
				pushed = 1;
				x++;
				if(x == LIGHTS){
					x = 0;
				}
			} 
			
			if(st->buttons & CONT_Y && pushed == 0){
				LIGHTS--;
				pushed = 1;
				if(LIGHTS < 0){
					LIGHTS = MAX_LIGHTS;
				}
			}
			
			if(st->buttons & CONT_B && pushed == 0){
				display_fps ^= 0x01;
				pushed = 1;
			}
			
			if(st->buttons & CONT_X && pushed == 0){
				bumpenabled ^= 0x01;
				pushed = 1;
			}
			
			if(!(st->buttons & CONT_A) && !(st->buttons & CONT_B) && !(st->buttons & CONT_X) && !(st->buttons & CONT_Y)){
				pushed = 0;
			}
			
		
		MAPLE_FOREACH_END();
		running_stats();
		sprintf(buf,"FPS:%f",avgfps);
		if(display_fps){
				//printf("%s\n",buf);
			bfont_draw_str(vram_s + (640*24),640,1,buf);
		}
		
	}
	DeleteTexture(&GlobalNormal);
	DeleteTexture(&GlobalTex);
	//sndoggvorbis_stop();
	//sndoggvorbis_shutdown();
	pvr_shutdown();
	return 0;
}