	
	!void normalize(void* vert1,void *vertnorm)
	!r4 = [arg] = @vert: struct {float x,y,z,w}
	!r5 = [arg] = @normalout: struct {float x,y,z,w}
	
	.globl _normalize
_normalize:
	pref @r4
	fschg
	fmov @r4+,dr0	! Load in vertex to be normalized
	fmov @r4+,dr2
	fschg
	fldi0 fr3		! Set W component to 0
	
	fipr fv0,fv0	! get the magnitude of the vector by finding the dot product with itself
	fsrra fr3		! inverse square root is faster on DC
	fmul fr3, fr0	! multiply out to components
	fmul fr3, fr1
	fmul fr3, fr2
	fldi1 fr3		! Set W component to 1
	
	add #32,r5		! Setup out pointer
	fschg
	fmov dr2, @-r5	! Save to out vector
	fmov dr0, @-r5
	rts
	fschg
	
	
	
	!void lightvert(void* vert, void*light,void *outcolor,void *vert_normal)
	!r4 = [arg] = @vert: struct{ float x,float y,float z,float w }
	!r5 = [arg] = @light: struct {float x,y,z,w, ac,ab,aa,dummy, r,g,b,a }
	!r6 = [arg] = @outcolor: struct { float r,g,b,a }
	!r7 = [arg] = @vert_normal: struct {float x,y,z,w }
	
	.globl __lightvertex
	
__lightvertex:

	fschg
		
	fmov @r5+, dr0	!Move light position into fv0
	fmov @r5+, dr2

	fmov @r4+, dr4	!move vertex position into fv4
	fmov @r4+, dr6
	fldi0 fr7

	fmov @r7+, dr12	!move vertex normal into fv12
	fmov @r7+, dr14
	fldi0 fr15
	fschg
	
	fsub fr4, fr0	!Calculate light_pos - vertex pos
	fsub fr5, fr1
	fsub fr6, fr2
	fldi0 fr3
	
	fipr fv0,fv0 	!get the magnitude of light_pos - vertexpos
	fsrra fr3		!normalize
	fmul fr3, fr0
	fmul fr3, fr1
	fmul fr3, fr2 
	
	fmov fr3, fr7	!copy 1/magnitude of final pos into final pos in fv8 for later use as d in atten calc

	
	fldi0 fr3		! set the w comp of normalized final pos to 0
	fipr fv0, fv12	!calculate the dot product of vertex normal and  final pos normal
	
	
	fcmp/gt fr15, fr3	!make sure its above 0
	bf .max
	fldi0 fr15
.max:
	fschg
	fmov @r5+, dr4		!copy atten c and atten b into fr4-5
	fschg
	!fmov @r5+, fr5
	fmov @r5+, fr6
	add #4,r5

	fmul fr7, fr5		! linear
	fadd fr4, fr5		! linear + constant
	fmul fr7,fr7		! quadratic
	fmul fr6,fr7		! quadratic*a
	fadd fr5,fr7		! combine
	
	fschg
	fmov @r5+, dr0
	fschg
	fmov @r5+, fr2
	!fmov @r5+, fr0
	!fmov @r5+, fr1
	!fmov @r5+, fr2		!Move light color into fv0
	
	fmul fr15,fr0		! multiply light color by attenuation and final diffuse
	fmul fr7, fr0
	fmul fr15,fr1
	fmul fr7, fr1
	fmul fr15,fr2
	fmul fr7, fr2
	
	!fmov @r6+, fr4		! load in vertex color @ outcolor
	!fmov @r6+, fr5
	!fmov @r6+, fr6
	!fmov @r6+, fr7

	fschg
	fmov @r6+, dr4
	fmov @r6+, dr6
	fschg

	fldi1 fr3			! set fr3 to 1 for clamping
	fadd fr4,fr0		! Add original color with calculated color and clamp
	fcmp/gt fr0,fr3
	bt .nxt
	fldi1 fr0
.nxt:
	fadd fr5,fr1
	fcmp/gt fr1,fr3
	bt .nxt2
	fldi1 fr1
.nxt2:
	fadd fr6,fr2
	fcmp/gt fr2,fr3
	bt .nxt3
	fldi1 fr2
.nxt3:
	fschg
	fmov dr2, @-r6
	fmov dr0, @-r6
	!fmov.s fr3, @-r6		! Save final colour to outcolour
	!fmov.s fr2, @-r6
	!fmov.s fr1, @-r6
	rts
	fschg
	!fmov.s fr0, @-r6
	

	