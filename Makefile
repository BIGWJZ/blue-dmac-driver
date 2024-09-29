KERNEL_SRC_DIR ?= /linux-src

driver:
	cd ./src && make

qemu:
	./scripts/run_qemu.sh

gdb:
	./scripts/run_gdb.sh

# language server config, e.g., clangd
lsp:
	cd $(KERNEL_SRC_DIR) && ./scripts/clang-tools/gen_compile_commands.py
	cd src && $(KERNEL_SRC_DIR)/scripts/clang-tools/gen_compile_commands.py -d $(KERNEL_SRC_DIR) ./