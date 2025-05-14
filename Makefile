# Tools
NASM = nasm
GCC = gcc
LD = ld
OBJCOPY = objcopy
QEMU = qemu-system-i386
XORRISO = xorriso

# Flags
NASM_FLAGS = -f bin
GCC_FLAGS = -m32 -ffreestanding -fno-pie -c
LD_FLAGS = -m elf_i386 -T linker.ld
QEMU_FLAGS = -cdrom os-image.iso

# Output files
OS_IMAGE = os-image.iso
KERNEL_ELF = kernel.elf
KERNEL_BIN = kernel.bin
BOOT_BIN = boot.bin

# Source files
BOOT_ASM = boot.asm
KERNEL_ASM = kernel.asm
KERNEL_C = kernel.c
LINKER_SCRIPT = linker.ld

# Object files
KERNEL_ASM_OBJ = kernel_asm.o
KERNEL_C_OBJ = kernel_c.o

# Default target
all: $(OS_IMAGE)

# Create ISO with xorriso
$(OS_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	mkdir -p iso/boot
	cp $(BOOT_BIN) iso/boot/boot.bin
	cp $(KERNEL_BIN) iso/boot/kernel.bin
	$(XORRISO) -as mkisofs -b boot/boot.bin -no-emul-boot -boot-load-size 4 -o $(OS_IMAGE) iso
	rm -rf iso

# Convert kernel ELF to binary
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)

# Link kernel object files
$(KERNEL_ELF): $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ)
	$(LD) $(LD_FLAGS) $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ) -o $(KERNEL_ELF)

# Assemble bootloader
$(BOOT_BIN): $(BOOT_ASM)
	$(NASM) $(NASM_FLAGS) $(BOOT_ASM) -o $(BOOT_BIN)

# Assemble kernel entry
$(KERNEL_ASM_OBJ): $(KERNEL_ASM)
	$(NASM) -f elf32 $(KERNEL_ASM) -o $(KERNEL_ASM_OBJ)

# Compile kernel C code
$(KERNEL_C_OBJ): $(KERNEL_C)
	$(GCC) $(GCC_FLAGS) $(KERNEL_C) -o $(KERNEL_C_OBJ)

# Run in QEMU
run: $(OS_IMAGE)
	$(QEMU) $(QEMU_FLAGS)

# Clean build artifacts
clean:
	rm -f $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ) $(KERNEL_ELF) $(KERNEL_BIN) $(BOOT_BIN) $(OS_IMAGE)

.PHONY: all run clean