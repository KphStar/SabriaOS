# Custom x86 Operating System Kernel (Assembly Only)

This project is a minimal operating system written entirely in **x86 Assembly**, inspired by [AlexJMercer's x86_64-Bootloader-and-Kernel](https://github.com/AlexJMercer/x86_64-Bootloader-and-Kernel). It features a handcrafted **bootloader**, **real-mode BIOS input**, **GDT setup**, and a simple **protected-mode shell** kernel.

## 🧠 Features

- 🪵 **Custom Bootloader**:
  - Starts in real mode at `0x7C00`
  - Prompts for user input: `Welcome to the void. Your name?`
  - Echoes typed characters via BIOS
  - Greets user with: `Hi <name>`
  - Loads the kernel (`kernel.asm`) from disk (sector 2+)

- 🔐 **GDT & Protected Mode**:
  - Sets up GDT for flat 32-bit segments
  - Switches to protected mode via far jump to `0x1000`

- 🧠 **32-bit Kernel**:
  - Written in pure Assembly
  - Clears the screen
  - Implements a basic shell using VGA text mode
  - Supports commands:
    - `PRINT`: Displays a message
    - `HALT`: Halts the system

- ⌨️ **Keyboard Input**:
  - In protected mode: Uses port `0x60` to read raw scancodes
  - Translates them using a lookup table.
  - using qemu-system-i386 -cdrom os-image.iso -vga std
  - docker run -it -v $(pwd):/work myos-build bash
