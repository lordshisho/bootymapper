CFLAGS+=-I../../lib/ -Wall
LDFLAGS+=-lpcap -levent -levent_extra -levent_core -lm
VPATH=/../../lib/

# from dpkg-buildflags --get CFLAGS, but use stack-protector-all and fPIC
GCCHARDENING=-g -O2 -fstack-protector-all --param=ssp-buffer-size=4 -Wformat -Wformat-security -Werror=format-security -fPIC
# from gpkg-buildflags --get LDFLAGS, + -z,now
LDHARDENING=-Wl,-Bsymbolic-functions -Wl,-z,relro,-z,now -LLIBDIR

CFLAGS+=$(GCCHARDENING)
LDFLAGS+=$(LDHARDENING)


all: bootymapper

bootymapper: bootymapper.o logger.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f bootymapper *.o
