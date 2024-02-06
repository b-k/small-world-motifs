LDLIBS = -lapophenia -lgsl -lgslcblas -lsqlite3 #-pg
#CC = clang

CC=gcc
#CFLAGS=-g -Wall -ldb -lgsl -lapophenia -I/usr/include/lua5.4 -Wall
#CFLAGS=-g -Wall -I/usr/include/lua5.4
#LDLIBS=-ldb -lgsl -lapophenia -llua5.4 #-pg

#LDLIBS = -lapophenia -lgsl -lgslcblas -lsqlite3 #-pg
#CC = clang
#CFLAGS=-g -Wall -ldb -lgsl -lapophenia -I/usr/include -I/usr/include/lua5.4/ -Wall -O3
#LDLIBS=-L/home/rf4qb/.a/tech/root/lib -ldb -lgsl -lapophenia -llua -v

CFLAGS=-Og -g -Wall -ldb -lgsl -lapophenia -I/usr/include/lua5.4 -Wall #-pg
LDLIBS=-ldb -lgsl -lapophenia -llua5.4 #-pg

ifndef OUT
	OUT := out
endif

go: netlib.so
	mkdir -p $(OUT)/
	lua5.4 go.lua $(OUT)
	lua5.4 queries.lua $(OUT)
	#lua5.4 queries.lua $(OUT)

netlib.so: draws.o motif.o netlib.o dual.o #draws.c dual.c motif.c
	 $(CC) $(CFLAGS) -shared -Wno-undef -o $@ $^ $(LDLIBS)

motif.o: motif.c
	 $(CC) $(CFLAGS) -fPIC  -o $@ -c $< $(LDLIBS)

dual.o: dual.c
	 $(CC) $(CFLAGS) -fPIC  -o $@ -c $< $(LDLIBS)

draws.o: draws.c
	 $(CC) $(CFLAGS) -fPIC  -o $@ -c $< $(LDLIBS)

netlib.o: netlib.c
	 $(CC) $(CFLAGS) -fPIC  -o $@ -c $< $(LDLIBS)

dock:
	docker build -t luanets -f Dockerfile .

shell:
	docker run -v "`pwd`":/home/ldb/@ -it luanets zsh

clean:
	rm -rf main out/

score:
	cat out/out.lua.psv | sed 's/|.*//' | sort | uniq -c | sort -n

pngs:
	mkdir -p outdots/u
	mkdir -p outdots/m
	mkdir -p outdots/mm
	#./final_script | sed -f to_dot.sed output_attic/symsib.pcts | sort -u | awk 'BEGIN {a=0} {sub("XXX", a); a++; print}' > outdots/dots.printer
	./final_script | sed -f to_dot.sed | sort -u -k 8 -n -r | awk 'BEGIN {a=0} {sub("XXX", a); a++; print}' > outdots/dots.printer
	sh outdots/dots.printer


