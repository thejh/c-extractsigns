all: libnbt
	gcc -std=c99 -O2 -g extractsigns.c -L lib/cNBT -lnbt -lz -o extractsigns

libnbt:
	$(MAKE) $(MAKEFLAGS) -C lib/cNBT
