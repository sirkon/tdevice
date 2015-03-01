OPTS = -g -O0
LPATH =
IPATH =
CC = gcc
COMPILE = $(CC) $(OPTS) $(LPATH) $(IPATH)

all: targetdevice.o confparser.o maplib.o binder.o network.o processors.o daemon.c
		$(COMPILE) -o targetdevice daemon.c targetdevice.o confparser.o maplib.o binder.o network.o processors.o -lyaml -lcrypto -lssl -lbsd

targetdevice.o: targetdevice.c
		$(CC) $(OPTS) $(LPATH) $(IPATH) -c targetdevice.c

confparser.o: confparser.c
		$(CC) $(OPTS) $(LPATH) $(IPATH) -c confparser.c

maplib.o: maplib.c
		$(CC) $(OPTS) $(LPATH) $(IPATH) -c maplib.c

binder.o: binder.c
		$(COMPILE) -c binder.c

processors.o: processors.c
		$(COMPILE) -c processors.c

network.o: network.c
		$(COMPILE) -c network.c

clean:
		rm -f *.o targetdevice *_test


# This is for test purposes only

confparser_test: confparser.o maplib.o confparser_test.c
		$(CC) $(OPTS) $(LPATH) $(IPATH) -o confparser_test confparser_test.c confparser.o maplib.o -lyaml


maplib_test: maplib.o maplib_test.c
		$(CC) $(OPTS) $(LPATH) $(IPATH) -o maplib_test maplib_test.c maplib.o


binder_test: maplib.o confparser.o binder.o binder_test.c
		$(COMPILE) -o binder_test maplib.o confparser.o binder.o binder_test.c -lyaml

processors_test: maplib.o processors.o processors_test.c
		$(COMPILE) -o processors_test maplib.o processors.o processors_test.c

network_test: network.o maplib.o confparser.o network_test.c
		$(COMPILE) -o network_test network.o maplib.o confparser.o network_test.c -lcrypto -lssl -lyaml
