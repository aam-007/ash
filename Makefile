# Makefile - MaximusOS with Persistence

all: kernel.bin

boot.o: boot.asm
	nasm -f elf32 boot.asm -o boot.o

kernel.o: kernel.c
	gcc -m32 -c kernel.c -o kernel.o -ffreestanding -O2 -Wall -Wextra

kernel.bin: boot.o kernel.o linker.ld
	ld -m elf_i386 -T linker.ld -o kernel.bin -nostdlib boot.o kernel.o

iso: kernel.bin
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/phonexos.bin
	echo 'menuentry "PhonexOS" {' > isodir/boot/grub/grub.cfg
	echo '	multiboot /boot/phonexos.bin' >> isodir/boot/grub/grub.cfg
	echo '}' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o phonexos.iso isodir

# Create a 1MB blank disk image filled with zeros
disk.img:
	dd if=/dev/zero of=disk.img bs=512 count=2048

# Run QEMU with the Hard Drive attached (-hda)
run: iso disk.img
	qemu-system-i386 -cdrom phonexos.iso -hda disk.img -display curses

clean:
	rm -f *.o *.bin *.iso *.img
	rm -rf isodir
