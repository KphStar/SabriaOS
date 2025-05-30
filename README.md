# 🚀 Sebria OS - x86 Operating System Kernel (C & Assembly)

Sebria OS is a minimal 32-bit operating system built from scratch in **C** and **x86 Assembly**, featuring a custom bootloader, VGA-based shell interface, multitasking, and a virtual file system. It is designed for educational purposes and runs inside QEMU.

> Inspired by projects like [AlexJMercer's Bootloader/Kernel](https://github.com/AlexJMercer/x86_64-Bootloader-and-Kernel)

---

## 🔧 Architecture Overview

### 🔹 Bootloader (Assembly)

* Real mode entry at `0x7C00`
* BIOS-based input and output
* Loads kernel from sector 2 onward
* Switches to protected mode using a GDT

### 🔹 Kernel (C + Assembly)

* VGA text mode interface (80x25)
* VGA color-coded shell with UI windows
* Paging setup with flat memory model
* Cooperative multitasking via a priority-based scheduler
* User-space syscall simulation via `int 0x80`

---

## 💻 Shell Features

| Command        | Description                        |
| -------------- | ---------------------------------- |
| `print`        | Prints a test message              |
| `ls`           | Lists all files in the VFS         |
| `touch <file>` | Create and write to a new file     |
| `cat <file>`   | Paginate through file contents     |
| `diary`        | Opens a text UI to save notes      |
| `ps`           | Shows running processes            |
| `kill <pid>`   | Terminates a process by PID        |
| `dump`         | Displays screen buffer contents    |
| `virtual`      | Shows virtual memory and file info |
| `clear`        | Clears the shell display area      |
| `halt`         | Halts the OS                       |

---

## 📁 Virtual File System (VFS)

* Simple in-memory structure
* Max 8 inodes (`MAX_INODES = 8`)
* Max 2048 bytes per file (`MAX_FILE_SIZE = 2048`)
* Supports `create`, `open`, `read`, `write`, `close`, `ls`

---

## 🔁 Multitasking

* Supports up to 8 processes
* Priority-based scheduling
* Kernel and simulated user-level processes

---

## 🧪 Building & Running

### 🐳 Using Docker

```bash
docker run -it -v $(pwd):/work myos-build bash
make run  # or manually: qemu-system-i386 -cdrom os-image.iso -vga std
```

### 🔧 Requirements

* `i686-elf-gcc`
* `nasm`
* `qemu-system-i386`

---

## 🚧 Future Work

* Disk-backed persistence (FAT/ext2)
* More robust paging and process isolation
* Basic I/O streams and redirection
* Support for external modules and drivers

---

## 📜 License

MIT License

---

## 👤 Author

**Duc Le**
San Diego State University — Computer Science

---
