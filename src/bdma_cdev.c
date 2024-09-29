#include "linux/fs.h"
#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

#include "bdma_cdev.h"
#include "libbdma.h"

struct class *g_bdma_class;

// TODO: realize a new file_ops, whcih receives va head and length from user
// space, pins the pages and writes pa's to the hw

static ssize_t ctrl_read(struct file *file, char __user *buf, size_t count,
                         loff_t *pos) {
  struct bdma_dev *bdev = (struct bdma_dev *)file->private_data;
  void __iomem *reg;
  u32 w;
  int rv;

  /* only 32-bit aligned and 32-bit multiples */
  if (*pos & 3)
    return -EPROTO;

  reg = bdev->bar0 + *pos;
  w = ioread32(reg);

  rv = copy_to_user(buf, &w, 4);
  if (rv) {
    pr_err("Copy to userspace failed!\n");
    return -EINVAL;
  }

  *pos += 4;
  return 4;
}

static ssize_t ctrl_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *pos) {
  struct bdma_dev *bdev = (struct bdma_dev *)file->private_data;
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

  pr_info("%s(0x%08x @%p, count=%ld, pos=%d)\n", __func__, w, reg, (long)count,
          (int)*pos);

  iowrite32(w, reg);
  *pos += 4;
  return 4;
}

static int char_open(struct inode *inode, struct file *file) {
  struct bdma_cdev *bcdev;

  /* pointer to containing structure of the character device inode */
  bcdev = container_of(inode->i_cdev, struct bdma_cdev, cdev);
  if (bcdev->magic != MAGIC_CHAR) {
    pr_err("xcdev 0x%p inode 0x%lx magic mismatch 0x%lx\n", bcdev, inode->i_ino,
           bcdev->magic);
    return -EINVAL;
  }
  /* create a reference to our char device in the opened file */
  file->private_data = bcdev;

  return 0;
}

static int char_close(struct inode *inode, struct file *file) {
  struct bdma_dev *bdev;
  struct bdma_cdev *bcdev = (struct bdma_cdev *)file->private_data;

  if (!bcdev) {
    pr_err("char device with inode 0x%lx bcdev NULL\n", inode->i_ino);
    return -EINVAL;
  }

  if (bcdev->magic != MAGIC_CHAR) {
    pr_err("xcdev 0x%p magic mismatch 0x%lx\n", bcdev, bcdev->magic);
    return -EINVAL;
  }

  /* fetch device specific data stored earlier during open */
  bdev = bcdev->bdev;
  if (!bdev) {
    pr_err("char device with inode 0x%lx bdev NULL\n", inode->i_ino);
    return -EINVAL;
  }

  return 0;
}

static const struct file_operations ctrl_fops = {.owner = THIS_MODULE,
                                                 .open = char_open,
                                                 .release = char_close,
                                                 .read = ctrl_read,
                                                 .write = ctrl_write};

// Only support ctrl cdev now
// bdma_control is a raw interface for registers visiting
static int create_bcdev(struct bdma_dev *bdev, struct bdma_cdev *bcdev, int bar,
                        struct class *class) {
  int rv = 0;
  dev_t dev;

  spin_lock_init(&bcdev->lock);

  if (!bdev->major) {
    int rv = alloc_chrdev_region(&dev, BDMA_MINOR_BASE, BDMA_MINOR_COUNT,
                                 BDMA_NODE_NAME);
    if (rv) {
      pr_err("fail to allocate char dev region %d.\n", rv);
      return rv;
    }
    bdev->major = MAJOR(dev);
  }

  bcdev->magic = MAGIC_CHAR;
  bcdev->cdev.owner = THIS_MODULE;
  bcdev->bdev = bdev;
  bcdev->bar = bar;

  cdev_init(&bcdev->cdev, &ctrl_fops);
  bcdev->cdevno = MKDEV(bdev->major, 0);

  rv = cdev_add(&bcdev->cdev, bcdev->cdevno, 1);
  if (rv) {
    pr_err("cdev add failed %d", rv);
    goto unregister_region;
  }

  bcdev->sys_device = device_create(class, &bdev->pdev->dev, bcdev->cdevno,
                                    NULL, "bdma_control");
  if (!bcdev->sys_device) {
    pr_err("device_create bdma_control failed!\n");
    rv = -EPERM;
    goto del_cdev;
  }

  return rv;

del_cdev:
  cdev_del(&bcdev->cdev);
unregister_region:
  unregister_chrdev_region(bcdev->cdevno, BDMA_MINOR_COUNT);
  return rv;
}

int bdev_create_interfaces(struct bdma_dev *bdev) {
  int rv = 0;

  g_bdma_class = class_create(THIS_MODULE, BDMA_NODE_NAME);
  if (IS_ERR(g_bdma_class)) {
    rv = PTR_ERR(g_bdma_class);
    pr_err("Failed to create class.\n");
  }

  rv = create_bcdev(bdev, &bdev->ctrl_cdev, 0, g_bdma_class);
  if (rv) {
    pr_err("create char file failed %d", rv);
  }

  // Maybe we will create more bcdev here

  return rv;
}

void bdev_destroy_interfaces(struct bdma_dev *bdev) {
  struct bdma_cdev bcdev = bdev->ctrl_cdev;

  if (bcdev.sys_device)
    device_destroy(g_bdma_class, bcdev.cdevno);

  cdev_del(&bcdev.cdev);

  if (bdev->major)
    unregister_chrdev_region(MKDEV(bdev->major, BDMA_MINOR_BASE),
                             BDMA_MINOR_COUNT);
}
