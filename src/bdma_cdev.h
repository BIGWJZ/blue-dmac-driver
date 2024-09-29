#ifndef __BDMA_CDEV_H__
#define __BDMA_CDEV_H__

#include "libbdma.h"
#include "linux/fs.h"
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/uaccess.h>

int bdev_create_interfaces(struct bdma_dev *bdev);
void bdev_destroy_interfaces(struct bdma_dev *bdev);

#endif
