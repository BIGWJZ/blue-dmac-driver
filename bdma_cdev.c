#include "bdma_cdev.h"

#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

static ssize_t ctrl_read(struct file *fp, char __user *buf, size_t count,
                         loff_t *pos) {
  struct bdma_dev *bdev = (struct bdma_dev *)fp->private_data;
  void __iomem *reg;
  u32 w;
  int rv;

  /* only 32-bit aligned and 32-bit multiples */
  if (*pos & 3)
    return -EPROTO;

  reg = bdev->bar0 + *pos;
  w = ioread32(reg);
  dbg_io("%s(@%p, count=%ld, pos=%d) value = 0x%08x\n", __func__, reg,
         (long)count, (int)*pos, w);

  rv = copy_to_user(buf, &w, 4);
  if (rv)
    dbg_io("Copy to userspace failed but continuing\n");

  *pos += 4;
  return 4;
}

static ssize_t ctrl_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *pos) {
  struct bdma_dev *bdev = (struct bdma_dev *)fp->private_data;
  void __iomem *reg;
  u32 w;
  int rv;

  /* only 32-bit aligned and 32-bit multiples */
  if (*pos & 3)
    return -EPROTO;

  reg = bdev->bar0 + *pos;
  rv = copy_from_user(&w, buf, 4);
  if (rv)
    pr_info("copy from user failed %d/4, but continuing.\n", rv);

  dbg_io("%s(0x%08x @%p, count=%ld, pos=%d)\n", __func__, w, reg, (long)count,
         (int)*pos);

  iowrite32(w, reg);
  *pos += 4;
  return 4;
}

int create_bdma_cdev(struct bdma_dev *bdev) {
  int rv;
  struct class *bdma_class;
  dev_t dev;

  rv = alloc_chrdev_region(&dev, BDMA_MINOR_BASE, BDMA_MINOR_COUNT,
                           BDMA_NODE_NAME);
  if (rv < 0) {
    pr_err("alloc char device region failed %d.\n", rv);
    return rv;
  }

  bdma_class = class_create(THIS_MODULE, BDMA_NODE_NAME);
  if (IS_ERR(bdma_class)) {
    rv = PTR_ERR(bdma_class);
    pr_err("Failed to create class.\n");
    goto err_class;
  }

  device_create(bdma_class, NULL, dev, NULL, "ctrl");

  cdev_init(&bdev->cdev, &ctrl_fops);
  bdev->cdev.owner = THIS_MODULE;

  rv = cdev_add(&bdev->cdev, dev, 1);
  if (rv < 0)
    goto err_cdev;

  return 0;

err_cdev:
  pr_err("Failed to add cdev.\n");
  device_destroy(bdma_class, dev);
  class_destroy(bdma_class);
err_class:
  unregister_chrdev_region(dev, 1);
  return rv;
}

// TODO
static const struct file_operations ctrl_fops = {
    .owner = THIS_MODULE,
    .open = bdma_open,
    .release = bdma_release,
    .read = ctrl_read,
    .write = ctrl_write,
    .poll = bdma_poll,
};