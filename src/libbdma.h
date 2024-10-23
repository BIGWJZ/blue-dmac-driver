#ifndef __LIBBDMA_H__
#define __LIBBDMA_H__

#include "linux/spinlock_types.h"
#include <linux/aio.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/uio.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#define debug pr_info;

/* obtain the 32 most significant (high) bits of a 32-bit or 64-bit address */
#define PCI_DMA_H(addr) ((addr >> 16) >> 16)
/* obtain the 32 least significant (low) bits of a 32-bit or 64-bit address */
#define PCI_DMA_L(addr) (addr & 0xffffffffUL)

#define BDMA_NODE_NAME "bdma"
#define BDMA_MINOR_BASE (0)
#define BDMA_MINOR_COUNT (255)
#define BDMA_REG_BYTES (4)
#define BDMA_CHANNEL_NUM_MAX (2)
#define BDMA_CHANNEL_REG_BYTES (BDMA_REG_BYTES * 6)

#define BDMA_REG_REGION (512)
#define BDMA_MAX_MR_PAGE_NUM BDMA_REG_REGION

#define BDMA_ENGINE_READ (0)
#define BDMA_ENGINE_WRITE (1)
#define BDMA_ENGINE_MR (2)

#define MAGIC_CHAR 0xBBBBBBBBUL

struct bdma_cdev {
  unsigned long magic;
  struct bdma_dev *bdev;
  dev_t cdevno;
  struct cdev cdev;
  int bar;
  unsigned long base;
  struct device *sys_device;
  struct bdma_engine *engine;
  spinlock_t lock;
};

struct engine_regs {
  u32 doorbell;
  u32 desc_va_lo;
  u32 desc_va_hi;
  u32 desc_bytes;
  u32 poll_va_lo;
  u32 poll_va_hi;
};

struct mem_region {
  u64 head_page;
  u64 tail_page;
};

struct sw_desc {
  unsigned long long addr;
  size_t length;
  u32 control;
};

struct bdma_engine {
  int id;
  int channel;
  struct engine_regs *regs;
  u8 *pa_bus_addr;
  struct bdma_dev *bdev;

  u8 *poll_virt_addr;
  dma_addr_t poll_bus_addr;

  struct mem_region region;
  dma_addr_t pa_list[BDMA_MAX_MR_PAGE_NUM];
  int got_registerd_region;

  spinlock_t lock;
};

struct bdma_dev {
  struct pci_dev *pdev; /* pci device struct from probe() */

  const char *mod_name;
  int major;
  int c2h_channel_num;

  void __iomem *bar0;
  int got_regions;
  int regions_in_use;

  struct bdma_cdev ctrl_cdev; /* character device structures */
  struct bdma_cdev engine_cdev[BDMA_CHANNEL_NUM_MAX];
  dev_t bdevno;

  struct bdma_engine engines[BDMA_CHANNEL_NUM_MAX];
};

struct bdma_poll_wb {
  u32 status;
  dma_addr_t addr;
  u32 length;
};

struct bdma_dev *create_bdma_device(const char *mod_name, struct pci_dev *pdev);
void remove_bdma_device(struct pci_dev *pdev, void *dev_hndl);
int submit_transfer(unsigned long long user_addr, size_t length, u32 control,
                    struct bdma_engine *engine);
int memory_register(unsigned long long user_addr, size_t user_len,
                    struct bdma_engine *engine);

#endif
