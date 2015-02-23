##
##	SORRY FOR THE MESS I SUCK AT MAKEFILES!
##	-Liam "Dragmire" Ewasko

all:  rm-elf Game/main.elf stream

include $(KOS_BASE)/Makefile.rules


texconv = $(KOS_BASE)/utils/texconv-master/texconv
OBJS = light.o main.o

KOS_LOCAL_CFLAGS = -I$(KOS_BASE)/addons/zlib \
					-I$(KOS_BASE)/addons/oggvorbis \
					-I$(KOS_BASE)/lib \
					-I / 

KOS_CFLAGS += -O3 
			

clean:
	-rm -f Game/main.elf $(OBJS)
	-rm -f romdisk.*
#	-rm -f romdisk/*.raw
#	-rm -f romdisk/*.pal
	
stream:
	dc-tool -t 192.168.1.4 -r
	dc-tool -t 192.168.1.4 -c later -x Game/main.elf

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
	 #-rm -f Game/main.elf
	 -rm -f Game/main.bin

Game/main.elf: $(OBJS)  romdisk.o
	$(KOS_CC)  $(KOS_CFLAGS) -O2 $(KOS_LDFLAGS) -o $@ $(KOS_START) $^  -lkosutils  -loggvorbisplay -lpng -lz -lk++ -lstdc++  -lm   $(KOS_LIBS)

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
