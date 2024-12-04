TARGET_MODULE:=bdma

$(TARGET_MODULE)-objs := libbdma.o bdma_cdev.o bdma_mod.o
obj-m := $(TARGET_MODULE).o
BUILDSYSTEM_DIR := /linux-src
# BUILDSYSTEM_DIR := /lib/modules/$(shell uname -r)/build
PWD:=$(shell pwd)

all :
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) clean

# language server config, e.g., clangd
lsp:
	cd $(KERNEL_SRC_DIR) && ./scripts/clang-tools/gen_compile_commands.py
	cd blue-dmac-driver && $(KERNEL_SRC_DIR)/scripts/clang-tools/gen_compile_commands.py -d $(KERNEL_SRC_DIR) ./
