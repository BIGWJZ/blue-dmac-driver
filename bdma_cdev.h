#ifndef __BDMA_CDEV_H__
#define __BDMA_CDEV_H__

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "bdma_mod.h"

#define BDMA_NODE_NAME "bdma"
#define BDMA_MINOR_BASE (0)
#define BDMA_MINOR_COUNT (255)

int create_bdma_cdev(struct pci_dev *pdev);
void destroy_bdma_cdev(void);

#endif
