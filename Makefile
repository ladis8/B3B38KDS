CC = gcc
LINKER = $(CC)
CFLAGS = -g -std=gnu99 -Wall -o1 -pedantic
LDFLAGS = -lssl -lcrypto

SOURCES1 = \
		  c_client.c \
		  utils.c \

SOURCES2 = \
		  c_client_StopAndWait.c \
		  utils.c \

TARGET1 = client_SelectiveRepeat
TARGET2 = client_StopAndWait

OBJECTS1 = $(filter %.o,$(SOURCES1:%.c=%.o))
OBJECTS2 = $(filter %.o,$(SOURCES2:%.c=%.o))

%.o: %.c
	@echo CC compiling  $<
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: all
all: $(TARGET2) $(TARGET1)

$(TARGET1): $(OBJECTS1)
	@echo LINKING...
	$(LINKER) $(OBJECTS1) $(LDFLAGS) -o $@
	
$(TARGET2): $(OBJECTS2)
	@echo LINKING...
	$(LINKER) $(OBJECTS2) $(LDFLAGS) -o $@
clean:
	rm -f *.o *.a $(OBJECTS1) $(OBJECTS2) $(TARGET1) $(TARGET2)


