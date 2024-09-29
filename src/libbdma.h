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

#define BDMA_NODE_NAME "bdma"
#define BDMA_MINOR_BASE (0)
#define BDMA_MINOR_COUNT (255)

#define MAGIC_CHAR 0xBBBBBBBBUL

struct bdma_cdev {
  unsigned long magic;
  struct bdma_dev *bdev;
  dev_t cdevno;
  struct cdev cdev;
  int bar;
  unsigned long base;
  struct device *sys_device;
  spinlock_t lock;
};

struct bdma_dev {
  struct pci_dev *pdev; /* pci device struct from probe() */

  const char *mod_name;
  int major;

  void __iomem *bar0; /* mapped BAR */
  int got_regions;
  int regions_in_use;

  struct bdma_cdev ctrl_cdev; /* character device structures */
  dev_t bdevno;
};

struct bdma_dev *create_bdma_device(const char *mod_name, struct pci_dev *pdev);
void remove_bdma_device(struct pci_dev *pdev, void *dev_hndl);

#endif
