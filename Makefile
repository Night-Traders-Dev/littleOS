CC = arm-none-eabi-gcc
AS = arm-none-eabi-as
LD = arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy

CFLAGS = -mcpu=cortex-m0plus -mthumb -O2 -Wall -ffreestanding
LDFLAGS = -T memmap.ld -nostdlib

all: littleos.uf2

littleos.elf: startup.o main.o uart.o shell.o
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(AS) -mcpu=cortex-m0plus -mthumb -c $< -o $@

littleos.uf2: littleos.elf
	# You need the 'elf2uf2' tool from Pico SDK or similar
	elf2uf2 $< $@

clean:
	rm -f *.o *.elf *.uf2
