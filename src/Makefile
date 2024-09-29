TARGET_MODULE:=bdma

$(TARGET_MODULE)-objs := libbdma.o bdma_cdev.o bdma_mod.o
obj-m := $(TARGET_MODULE).o
BUILDSYSTEM_DIR:=/linux-src
PWD:=$(shell pwd)

all :
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) clean