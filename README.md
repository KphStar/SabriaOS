🧵 Sebria OS - Custom x86 Operating System Kernel (C & Assembly)

Sebria OS is a handcrafted x86 operating system built from scratch using C and low-level Assembly, designed to run on QEMU with a custom shell, virtual file system, multitasking, and interactive UI via VGA text mode.

Inspired by low-level OS development projects like AlexJMercer's Bootloader/Kernel.

🧠 Features

🔧 Bootloader & Protected Mode

Custom Bootloader (in assembly)

Starts at 0x7C00 in real mode

Uses BIOS interrupts for input

Loads kernel from disk starting at sector 2

GDT Setup

Defines 32-bit flat memory segments

Switches to protected mode via far jump

🧑‍💻 Kernel (C Language)

VGA Text Interface (mode 0x03)

Displays colorful framed windows and shell interface

Shell Commands

print – Prints a message

ls – Lists files in the VFS

touch <filename> – Create & write to a file

cat <filename> – Read and page through a file

ps – Show running processes

kill <pid> – Terminate a process

clear – Clear the shell area

diary – Opens a text box to write notes to diary.txt

virtual – Displays VFS and process info

dump – Dumps screen content into a modal

halt – Halts the system

📁 Virtual File System (VFS)

In-memory file system with:

MAX_INODES = 8

MAX_FILE_SIZE = 2048 bytes per file

File operations:

create, open, read, write, close, list

Data is persisted during runtime only

🔁 Multitasking & Scheduler

Cooperative multitasking with schedule_flag

Process structure includes:

PID, priority, user/kernel privilege

Page directory support

Simulated user-space task using int 0x80 syscall

💾 Paging (Enabled)

Simple page directory and page table setup

Supports kernel/user separation (identity-mapped)

⌨️ Keyboard Support

Scancode to ASCII mapping via table

Handles shell input, file write UI, and diary input

Supports backspace, enter, escape, and navigation

🟦 Graphical Shell Enhancements

Color-coded windows and shell lines

Framed modals for editing text and viewing dumps

Uses VGA buffer directly (0xB8000)

🧪 Build & Run

🐳 Using Docker:

docker run -it -v $(pwd):/work myos-build bash
make run   # or manually: qemu-system-i386 -cdrom os-image.iso -vga std

🧰 Requirements

GCC cross-compiler (i686-elf-gcc)

NASM (for bootloader)

QEMU or Bochs for emulation

🚧 Planned Features

FAT32 or ext2 disk image support

Persistent file I/O via sector buffering

Virtual memory paging extensions

System call enhancements

File append mode, pipe simulation

📜 License

This project is free and open-source under the MIT License.

👤 Author

Duc Le – SDSU Computer Science
