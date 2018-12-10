CC = gcc
LINKER = $(CC)
CFLAGS = -g -std=gnu99 -Wall -o1 -pedantic
CFLAGS += -D${PORTS}
LDFLAGS = -lssl -lcrypto

PORTS = NORMAL


SOURCES1 = \
		  client_StopAndWait.c \
		  utils.c \

SOURCES2 = \
		  client_SelectiveRepeat.c \
		  utils.c \

SOURCES3 = \
		  server.c \
		  utils.c \

CLIENTTARGET1 = client_StopAndWait
CLIENTTARGET2 = client_SelectiveRepeat


SERVERTARGET = server

OBJECTS1 = $(filter %.o,$(SOURCES1:%.c=%.o))
OBJECTS2 = $(filter %.o,$(SOURCES2:%.c=%.o))
SERVEROBJECTS= $(filter %.o,$(SOURCES3:%.c=%.o))

ifeq ($(TARGET),STOPANDWAIT)
	CLIENTTARGET=$(CLIENTTARGET1)
	CLIENTOBJECTS=$(OBJECTS1)
endif
ifeq ($(TARGET),SELECTIVEREPEAT)
	CLIENTTARGET=$(CLIENTTARGET2)
	CLIENTOBJECTS=$(OBJECTS2)
endif

.PHONY: all
all: $(SERVERTARGET) $(CLIENTTARGET)

%.o: %.c
	@echo CC compiling  $<
	$(CC) -c $(CFLAGS) $< -o $@


$(CLIENTTARGET): $(CLIENTOBJECTS)
	@echo LINKING...
	$(LINKER) $(CLIENTOBJECTS) $(LDFLAGS) -o $@

$(SERVERTARGET): $(SERVEROBJECTS)
	@echo LINKING...
	$(LINKER) $(SERVEROBJECTS) $(LDFLAGS) -o $@
	
.PHONY: clean
clean:
	rm -f *.o *.a $(OBJECTS1) $(OBJECTS2) $(SERVEROBJECTS) $(CLIENTTARGET1) $(CLIENTTARGET2) $(SERVERTARGET)


