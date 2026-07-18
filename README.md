# Ash

A minimal, deterministic operating system built from first principles, featuring a custom bootloader, filesystem, and command-driven architecture designed for clarity and complete hardware control.

---

## Features

### Core System
- **32-bit protected mode kernel**
- **Custom Multiboot-compliant bootloader**
- **Clean and deterministic execution path**

### Display & Interface
- **VGA text mode renderer** with color support
- **Automatic scrolling**
- **Minimal interactive shell**

### Input
- **Keyboard driver** with shift handling
- **Raw scancode processing**
- **Line input with backspace support**

### Storage
- **ATA PIO driver** (read, write, flush)
- **Polling-based status handling**
- **Sector buffering** (512-byte data blocks)

### PhonexFS
A minimal filesystem designed specifically for ash:
- Directory table stored in a dedicated sector
- 16 file entries maximum
- Per-file sector allocation
- Read, save, and overwrite operations
- Format support

### Built-in Tools
- **File editor** (`write`)
- **File reader** (`read`)
- **Calculator** (basic arithmetic)
- **System info**
- **Reboot and poweroff routines**

---

## Shell Commands

| Command | Description |
|---------|-------------|
| `help` | Show all commands |
| `ls` | List files on disk |
| `write <name>` | Create or overwrite a file |
| `read <name>` | Display file contents |
| `calc` | Simple arithmetic (e.g., `calc 12 + 4`) |
| `format` | Wipe directory table |
| `reboot` | Reboot the machine |
| `poweroff` | Shut down QEMU/Bochs |
| `whoisthis` | Show OS version info |
| `clear` | Reset the terminal |

---

## Folder Structure

```
/
├── boot.asm        # Multiboot entry point
├── kernel.c        # Core OS logic
├── linker.ld       # Memory layout
├── Makefile        # Build + run commands
└── README.md       # Project documentation
```

---

## Build and Run

### Requirements
- `nasm` (Netwide Assembler)
- `gcc` (i386 cross-compiler preferred)
- `ld` (i386 linker)
- `qemu-system-i386` or `bochs` (x86 emulator)

### Build

```bash
make
```

### Run

```bash
make run
```

This will compile the OS and launch it in QEMU.

---

## Philosophy

**ash** is intentionally small so every subsystem is readable in a single sitting. The purpose is to give developers full visibility and control over:

- Boot flow
- Hardware drivers
- Memory layout
- I/O operations
- Disk operations
- Terminal rendering
- Command parsing

ash demonstrates how a complete micro operating system can be built from scratch with **zero dependencies**, while remaining accessible and modifiable.

---

## Versioning

**ash** – Version: `1.0-dev`

---

## License

This project is open source. Feel free to explore, modify, and learn from the code.

---


---

## Acknowledgments

Built with a focus on simplicity, clarity, and educational value for anyone interested in operating system development from the ground up.

---

