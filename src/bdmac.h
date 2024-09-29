#ifndef __BDMA_MODULE_H__
#define __BDMA_MODULE_H__

#include <linux/aio.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock_types.h>
#include <linux/splice.h>
#include <linux/types.h>
#include <linux/uio.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

// #include "bdma_cdev.h"

struct bdma_dev {
  struct pci_dev *pdev;  /* pci device struct from probe() */
  void __iomem *bar0;    /* mapped BAR */
  struct cdev ctrl_cdev; /* character device structures */
  dev_t bdevno;
};

#endif
