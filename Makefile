CC = gcc
LINKER = $(CC)
CFLAGS = -g -std=gnu99 -Wall -o1 -pedantic
CFLAGS += -D${PORTS}
LDFLAGS = -lssl -lcrypto

SOURCES1 = \
		  client_SelectiveRepeat.c \
		  utils.c \

SOURCES2 = \
		  client_StopAndWait.c \
		  utils.c \

SOURCES3 = \
		  server.c \
		  utils.c \

TARGET1 = client_SelectiveRepeat
TARGET2 = client_StopAndWait
TARGET3 = server

OBJECTS1 = $(filter %.o,$(SOURCES1:%.c=%.o))
	
OBJECTS2 = $(filter %.o,$(SOURCES2:%.c=%.o))

OBJECTS3 = $(filter %.o,$(SOURCES3:%.c=%.o))

%.o: %.c
	@echo CC compiling  $<
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: all
all: $(TARGET1) $(TARGET2) $(TARGET3)

$(TARGET1): $(OBJECTS1)
	@echo LINKING...
	$(LINKER) $(OBJECTS1) $(LDFLAGS) -o $@
	
$(TARGET2): $(OBJECTS2)
	@echo LINKING...
	$(LINKER) $(OBJECTS2) $(LDFLAGS) -o $@

$(TARGET3): $(OBJECTS3)
	@echo LINKING...
	$(LINKER) $(OBJECTS3) $(LDFLAGS) -o $@
clean:
	rm -f *.o *.a $(OBJECTS1) $(OBJECTS2)$(OBJECTS3) $(TARGET1) $(TARGET2) $(TARGET3)


