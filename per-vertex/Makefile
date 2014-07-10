##
##	SORRY FOR THE MESS I SUCK AT MAKEFILES!
##	-Liam "Dragmire" Ewasko

all:  rm-elf Game/main.elf Game/1ST_READ.bin 

include $(KOS_BASE)/Makefile.rules


texconv = $(KOS_BASE)/utils/texconv-master/texconv
OBJS =  main.o

KOS_LOCAL_CFLAGS = -I$(KOS_BASE)/addons/zlib \
					-I$(KOS_BASE)/addons/oggvorbis \
					-I$(KOS_BASE)/lib \
					-I / 
					

clean:
	-rm -f Game/main.elf $(OBJS)
	-rm -f romdisk.*
#	-rm -f romdisk/*.raw
#	-rm -f romdisk/*.pal
	


dist:
	-rm -f $(OBJS)
	-rm -f romdisk.*
	$(KOS_STRIP) Game/main.elf
	
rm-elf:
	-rm -f Game/1ST_READ.bin
	-rm -f Game/main.elf
	-rm -f romdisk.*
	
Game/1ST_READ.bin:
	 sh-elf-objcopy  -O binary Game/main.elf Game/main.bin
	 scramble Game/main.bin Game/1ST_READ.bin
	 -rm -f Game/main.elf
	 -rm -f Game/main.bin

Game/main.elf: $(OBJS)  romdisk.o
	$(KOS_CCPLUS)  $(KOS_CPPFLAGS) -O2 $(KOS_LDFLAGS) -o $@ $(KOS_START) $^  -lkosutils  -loggvorbisplay -lpng -lz -lk++ -lstdc++  -lm   $(KOS_LIBS)

vq: Bump tex
Bump: 
	$(texconv) -i billy_NRM.png -o bumpmap.raw  -f BUMPMAP -c  -v -p bumpmapprev.png
	mv bumpmap.raw romdisk
tex:
	$(texconv) -i billy.jpg -o text.raw -f RGB565 -c  -v
	mv text.raw romdisk
	
romdisk.img:
	$(KOS_GENROMFS) -f $@ -d romdisk -v

romdisk.o: romdisk.img
	$(KOS_BASE)/utils/bin2o/bin2o $< romdisk $@

run: Game/main.elf
	$(KOS_LOADER) $<
