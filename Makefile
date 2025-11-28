CC      = arm-none-eabi-gcc
LD      = arm-none-eabi-ld
AS      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

# Source layout:
#  boot/startup.S
#  boot/boot.c
#  src/main.c
#  src/kernel.c
#  src/uart.c
#  src/shell/shell.c

CFLAGS  = -mcpu=cortex-m0plus -mthumb -O2 -Wall -ffreestanding -nostdlib \
          -Iinclude
LDFLAGS = -T memmap.ld

# Tell make where to search for sources
VPATH = boot:src:src/shell

# Object files to build
OBJS = startup.o boot.o main.o kernel.o uart.o shell.o

all: littleos.uf2

littleos.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

# C sources (searched via VPATH)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assembly with preprocessor (.S) (searched via VPATH)
%.o: %.S
	$(AS) -mcpu=cortex-m0plus -mthumb -c $< -o $@

littleos.bin: littleos.elf
	$(OBJCOPY) -O binary $< $@

littleos.uf2: littleos.elf
	elf2uf2 $< $@

clean:
	rm -f $(OBJS) littleos.elf littleos.bin littleos.uf2
