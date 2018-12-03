CC = gcc
LINKER = $(CC)
CFLAGS = -g -std=gnu99 -Wall -o1 -pedantic
LDFLAGS = -lssl -lcrypto

SOURCES = \
		  c_client.c \
		  utils.c \

TARGET = client 

OBJECTS += $(filter %.o,$(SOURCES:%.c=%.o))

%.o: %.c
	@echo CC compiling  $<
	$(CC) -c $(CFLAGS) $< -o $@

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo LINKING...
	$(LINKER) $(OBJECTS) $(LDFLAGS) -o $@
clean:
	rm -f *.o *.a $(OBJECTS) $(TARGET) 


