/*
	PVR LIGHTING /w BUMP MAPPING EXAMPLE
	- Uses the PowerVR's bumpmapping and a per-vertex lighting engine I made
	
	By: Liam "Dragmire" Ewasko
	2014
	

*/


#include <kos.h>
#include <math.h>
#include <oggvorbis/sndoggvorbis.h>
#include "light.h"
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



float smoothstep(float min, float max, float x){
	if(x < min)
		return 0.0;
	if(x >= max)
		return 1.0;
	x  = (x-min)/(max-min);
	
	return (-2.0*(x*x*x)) + \
			(3.0*(x*x));
}

inline void Cross(Vector3 *v1,Vector3* v2,Vector3 *out){
	out->x = (v1->y*v2->z) - (v1->z*v2->y);
	out->y = (v1->z*v2->x) - (v1->x*v2->z);
	out->z = (v1->x*v2->y) - (v1->y*v2->x);
	out->w = 1.0;
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
	normalize(&pos3,&qd->surfacenormal);

	for(i = 0;i< 4;i++){
		Vector3 temp;
		temp.w = 1.0;
		temp.x = qd->verts[i].p.x;
		temp.y = qd->verts[i].p.y;
		temp.z = qd->verts[i].p.z;
		_lightvertex(&temp,l,&qd->verts[i].FinalColor,&qd->surfacenormal);
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


void Draw_Bump(Quad *qd){
	int i;
	pvr_poly_cxt_t p_cxt;
	pvr_poly_hdr_t p_hdr;
	
	pvr_poly_cxt_txr(&p_cxt,PVR_LIST_TR_POLY,qd->mat.bumpmap.fmt,qd->mat.bumpmap.w,qd->mat.bumpmap.w,qd->mat.bumpmap.txt,PVR_FILTER_BILINEAR);
	//p_cxt.gen.alpha = PVR_ALPHA_ENABLE;
	p_cxt.gen.specular = PVR_SPECULAR_ENABLE;
	//p_cxt.txr.env = PVR_TXRENV_DECAL;
	pvr_poly_compile(&p_hdr,&p_cxt);
	p_hdr.cmd |= 4;
	Vector3 G;
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
	
	Vector3 D;
	D.x = (qd->verts[0].p.x+16) - G.x;
	D.y = (qd->verts[0].p.y+16) - G.y;
	D.z = (qd->verts[0].p.z) - G.z;
	float mag = frsqrt(fipr_magnitude_sqr(D.x,D.y,D.z,0.0));
	float T = mag*(PI*2);//f(frsqrt(fipr_magnitude_sqr(D.x,D.y,0.0,0.0) + (l->z*l->z)));//0.5 * 3.141592/2;
	
	float Q = (fast_atan2f(D.y,D.x));

	pvr_prim(&p_hdr,sizeof(pvr_poly_hdr_t));
	Uint32 oargb = pvr_pack_bump(1.0,T,Q);//getBumpParameters(T,Q,0.1);
	for(i=0;i<4;i++){
		qd->verts[i].trans.oargb = oargb;
		qd->verts[i].trans.argb = PVR_PACK_COLOR(1.0,0.0,0.0,0.0);
		pvr_prim(&qd->verts[i].trans,sizeof(pvr_vertex_t));

	}

}


void Draw_Quad(Quad* qd){
	int i;
	pvr_poly_cxt_t p_cxt;
	pvr_poly_hdr_t p_hdr;

	pvr_poly_cxt_txr(&p_cxt,PVR_LIST_OP_POLY,GlobalTex.fmt,GlobalTex.w,GlobalTex.h,GlobalTex.txt,PVR_FILTER_BILINEAR);
	p_cxt.gen.shading = PVR_SHADE_GOURAUD;
	pvr_poly_compile(&p_hdr,&p_cxt);
	pvr_prim(&p_hdr,sizeof(p_hdr)); // submit header
	float w = 1.0;
	
	for( i =0; i < 4;i++){
	
		qd->verts[i].trans.argb = PVR_PACK_COLOR(0.0,qd->verts[i].FinalColor.x,qd->verts[i].FinalColor.y,qd->verts[i].FinalColor.z);;
		qd->verts[i].trans.oargb = 0;
		//printf("Vert %i: %f,%f,%f\n",i,qd->verts[i].p.x,qd->verts[i].p.y,qd->verts[i].p.z);
		qd->verts[i].trans.x = qd->verts[i].p.x;
		qd->verts[i].trans.y = qd->verts[i].p.y;
		qd->verts[i].trans.z = qd->verts[i].p.z;
		qd->verts[i].trans.u = qd->verts[i].p.u;
		qd->verts[i].trans.v = qd->verts[i].p.v;
		qd->verts[i].trans.flags = qd->verts[i].p.flags;

		mat_trans_nodiv(qd->verts[i].trans.x,qd->verts[i].trans.y,qd->verts[i].trans.z,w);
		//printf("Transformed %i: %f,%f,%f\n",i,qd->verts[i].trans.x,qd->verts[i].trans.y,qd->verts[i].trans.z);
		pvr_prim(&qd->verts[i].trans,sizeof(pvr_vertex_t));
		qd->verts[i].FinalColor.x = 0;
		qd->verts[i].FinalColor.y = 0;
		qd->verts[i].FinalColor.z = 0; 
	}
}


void Init_Quad(Quad* qd,float x,float y,float z,float w,float h){
	qd->verts[0].p.x = x;
	qd->verts[0].p.y = y;
	qd->verts[0].p.z = z;
	qd->verts[0].p.flags = PVR_CMD_VERTEX;
	qd->verts[0].p.u = 0.0;
	qd->verts[0].p.v = 0.0;
	qd->verts[0].p.oargb = 0;

	qd->verts[0].c.x = 0.0;
	qd->verts[0].c.y = 0.0;
	qd->verts[0].c.z = 0.0;


	qd->verts[1].p.x = x+w;
	qd->verts[1].p.y = y;
	qd->verts[1].p.z = z;
	qd->verts[1].p.flags = PVR_CMD_VERTEX;
	qd->verts[1].p.u = 1.0;
	qd->verts[1].p.v = 0.0;
	qd->verts[1].p.oargb = 0;

	qd->verts[1].c.x = 0.0;
	qd->verts[1].c.y = 0.0;
	qd->verts[1].c.z = 0.0;

	qd->verts[2].p.x = x;
	qd->verts[2].p.y = y+h;
	qd->verts[2].p.z = z;
	qd->verts[2].p.flags = PVR_CMD_VERTEX;
	qd->verts[2].p.u = 0.0;
	qd->verts[2].p.v = 1.0;
	qd->verts[2].p.oargb = 0;

	qd->verts[2].c.x = 0.0;
	qd->verts[2].c.y = 0.0;
	qd->verts[2].c.z = 0.0;

	qd->verts[3].p.x = x+w;
	qd->verts[3].p.y = y+h;
	qd->verts[3].p.z = z;
	qd->verts[3].p.flags = PVR_CMD_VERTEX_EOL;
	qd->verts[3].p.u = 1.0;
	qd->verts[3].p.v = 1.0;
	qd->verts[3].p.oargb = 0;

	qd->verts[3].c.x = 0.0;
	qd->verts[3].c.y = 0.0;
	qd->verts[3].c.z = 0.0;

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
		Init_Quad(&Layer[i],x,y,z,64,64);
		x += 64.0;
		if(x == 640){
			x = 0.0;
			y += 64.0;
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
			Draw_Bump(&Layer[i]);
		}
	}
}



void Init(){
	/*
		Standard initialization
	*/
	vid_set_mode(DM_640x480,PM_RGB565);
	vid_border_color(0,255,0);
	/*pvr_init_params_t params = {
                { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_16, PVR_BINSIZE_0, PVR_BINSIZE_0 },
                1024*512,0
        };*/

	//pvr_init(&params);
	pvr_init_defaults();
	//Set palette to ARGB8888 format
	
	pvr_set_pal_format(PVR_PAL_ARGB8888);
	
	mat_identity();

	
	//Initialize ogg streamer
	//snd_stream_init();
	//sndoggvorbis_init();
	
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

	while(q == 0){
		mat_identity();
		//mat_perspective(XCENTER,YCENTER,COT_FOVY_2,ZNEAR,ZFAR);
	//	//Screen view matrix
		
//		//Perspective frustrum
		mat_apply(&fr_mat);
		
		vid_border_color(255,0,0);
		pvr_wait_ready();
		vid_border_color(0,255,0);
		pvr_scene_begin();
		pvr_list_begin(PVR_LIST_OP_POLY);
			Draw_Layer();
		pvr_list_finish();
		if(bumpenabled){
			pvr_list_begin(PVR_LIST_TR_POLY);
				Draw_Layer_Bump();
			pvr_list_finish();
		}


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
			
			
			if(!(st->buttons & CONT_A) && !(st->buttons & CONT_B) && !(st->buttons & CONT_X) && !(st->buttons & CONT_Y)){
				pushed = 0;
			}
			
			
		
		MAPLE_FOREACH_END();
		
	}
	DeleteTexture(&GlobalNormal);
	DeleteTexture(&GlobalTex);
	//sndoggvorbis_stop();
	//sndoggvorbis_shutdown();
	pvr_shutdown();
	return 0;
}