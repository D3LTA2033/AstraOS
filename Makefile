# =============================================================================
# AstraOS - Top-Level Build System
# =============================================================================
# Usage:
#   make              Build the kernel ELF binary
#   make iso          Build a bootable ISO image
#   make run          Build and boot in QEMU
#   make clean        Remove build artifacts
#
# Requirements:
#   - i686-elf cross-compiler (i686-elf-gcc, i686-elf-ld)
#   - NASM assembler
#   - grub-mkrescue + xorriso (for ISO generation)
#   - qemu-system-i386 (for testing)
# =============================================================================

# --- Target architecture (will support x86_64 in future phases) ---
ARCH       := x86
TARGET     := i686-elf

# --- Cross-compiler toolchain ---
CC         := $(TARGET)-gcc
AS         := nasm
LD         := $(TARGET)-ld
OBJCOPY    := $(TARGET)-objcopy

# --- Directories ---
ARCH_DIR   := arch/$(ARCH)
KERNEL_DIR := kernel
USER_DIR   := user
INC_DIR    := include
BUILD_DIR  := build
ISO_DIR    := iso

# --- Compiler flags ---
CFLAGS     := -std=c11 -ffreestanding -fno-builtin -nostdlib \
              -Wall -Wextra -Werror \
              -I$(INC_DIR) -I$(ARCH_DIR)/include \
              -O2 -g

ASFLAGS    := -f elf32
LDFLAGS    := -T $(ARCH_DIR)/linker.ld -nostdlib

# --- Source files ---
ASM_SRCS   := $(ARCH_DIR)/boot/boot.asm \
              $(ARCH_DIR)/boot/gdt_flush.asm \
              $(ARCH_DIR)/boot/isr.asm \
              $(ARCH_DIR)/boot/paging.asm \
              $(ARCH_DIR)/boot/context_switch.asm \
              $(ARCH_DIR)/boot/usermode.asm

C_SRCS     := $(KERNEL_DIR)/core/kernel.c \
              $(KERNEL_DIR)/core/gdt.c \
              $(KERNEL_DIR)/core/idt.c \
              $(KERNEL_DIR)/core/pic.c \
              $(KERNEL_DIR)/core/pmm.c \
              $(KERNEL_DIR)/core/vmm.c \
              $(KERNEL_DIR)/core/page_fault.c \
              $(KERNEL_DIR)/core/kheap.c \
              $(KERNEL_DIR)/core/scheduler.c \
              $(KERNEL_DIR)/core/syscall.c \
              $(KERNEL_DIR)/core/usermode.c \
              $(KERNEL_DIR)/drivers/vga/vga.c \
              $(KERNEL_DIR)/drivers/pit/pit.c \
              $(KERNEL_DIR)/drivers/keyboard/keyboard.c \
              $(KERNEL_DIR)/lib/kstring.c

# --- Object files (placed in build/) ---
ASM_OBJS   := $(patsubst %.asm,$(BUILD_DIR)/%.o,$(ASM_SRCS))
C_OBJS     := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))

# --- User-space programs ---
USER_BIN   := $(BUILD_DIR)/user/test_user.bin
USER_OBJ   := $(BUILD_DIR)/user/test_user_bin.o

OBJS       := $(ASM_OBJS) $(C_OBJS) $(USER_OBJ)

# --- Output ---
KERNEL_BIN := $(BUILD_DIR)/astra.kernel
ISO_FILE   := $(BUILD_DIR)/astraos.iso

# =============================================================================
# Targets
# =============================================================================

.PHONY: all iso run clean

all: $(KERNEL_BIN)

# Link all objects into the kernel ELF binary
$(KERNEL_BIN): $(OBJS)
	@echo "[LD]  $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "[OK]  Kernel binary built successfully"

# Assemble .asm files
$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "[ASM] $<"
	@$(AS) $(ASFLAGS) $< -o $@

# Compile .c files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "[CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# --- User-space build ---
# Compile user program to ELF, then extract flat binary, then wrap as linkable object
$(USER_BIN): $(USER_DIR)/test_user.c $(USER_DIR)/entry.asm $(USER_DIR)/user.ld
	@mkdir -p $(dir $@)
	@echo "[USR] Compiling user program..."
	@$(AS) -f elf32 $(USER_DIR)/entry.asm -o $(BUILD_DIR)/user/entry.o
	@$(CC) -std=c11 -ffreestanding -fno-builtin -nostdlib -Wall -Wextra -Werror \
		-O2 -c $(USER_DIR)/test_user.c -o $(BUILD_DIR)/user/test_user.o
	@$(LD) -T $(USER_DIR)/user.ld -nostdlib -o $(BUILD_DIR)/user/test_user.elf \
		$(BUILD_DIR)/user/entry.o $(BUILD_DIR)/user/test_user.o
	@$(OBJCOPY) -O binary $(BUILD_DIR)/user/test_user.elf $@
	@echo "[USR] User binary: $@ ($$(stat -c%s $@) bytes)"

# Embed the flat binary as a linkable .o with symbols
$(USER_OBJ): $(USER_BIN)
	@echo "[USR] Embedding user binary into kernel..."
	@cd $(BUILD_DIR) && $(LD) -r -b binary -o $(CURDIR)/$@ user/test_user.bin

# Build bootable ISO using GRUB
iso: $(KERNEL_BIN)
	@echo "[ISO] Building bootable ISO..."
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_BIN) $(ISO_DIR)/boot/astra.kernel
	@cp config/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	@grub-mkrescue -o $(ISO_FILE) $(ISO_DIR) 2>/dev/null
	@echo "[OK]  ISO built: $(ISO_FILE)"

# Boot in QEMU
run: iso
	@echo "[QEMU] Booting AstraOS..."
	qemu-system-i386 -cdrom $(ISO_FILE) -m 128M -serial stdio

# Boot with debug output (no graphical window)
run-nographic: iso
	qemu-system-i386 -cdrom $(ISO_FILE) -m 128M -nographic

clean:
	@echo "[CLEAN] Removing build artifacts..."
	@rm -rf $(BUILD_DIR) $(ISO_DIR)
	@echo "[OK]   Clean complete"
