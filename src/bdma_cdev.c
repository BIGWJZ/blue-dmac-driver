#include "linux/cdev.h"
#include "linux/fs.h"
#include "linux/printk.h"
#include "linux/types.h"
#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

#include "bdma_cdev.h"
#include "libbdma.h"

struct class *g_bdma_class;

enum cdev_type {
  CHAR_CONTROL,
  CHAR_ENGINE,
};

static const char *const devnode_names[] = {
    BDMA_NODE_NAME "_control%d",
    BDMA_NODE_NAME "_c2h_%d",
};

static ssize_t ctrl_read(struct file *file, char __user *buf, size_t count,
                         loff_t *pos) {
  struct bdma_cdev *bcdev = (struct bdma_cdev *)file->private_data;
  void __iomem *reg;
  u32 w;
  int rv;

  /* only 32-bit aligned and 32-bit multiples */
  if (*pos & 3)
    return -EPROTO;

  reg = bcdev->bdev->bar0 + *pos;
  pr_info("pointer check: bar0 @ %px, pos %lld, expected %px, reg %px\n",
          bcdev->bdev->bar0, *pos, bcdev->bdev->bar0 + 4, reg);

  if (!reg) {
    pr_err("Invalid reg of bdma device: bar0: %p, reg:%p\n", bcdev->bdev->bar0,
           reg);
    return -EINVAL;
  }

  w = ioread32(reg);

  rv = copy_to_user(buf, &w, 4);
  if (rv) {
    pr_err("Copy to userspace failed!\n");
    return -EINVAL;
  }

  pr_info("%s(0x%08x @%p, count=%ld, pos=%d)\n", __func__, w, reg, (long)count,
          (int)*pos);

  *pos += 4;
  return 4;
}

static ssize_t ctrl_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *pos) {
  struct bdma_cdev *bcdev = (struct bdma_cdev *)file->private_data;
  void __iomem *reg;
  u32 w;
  int rv;

  /* only 32-bit aligned and 32-bit multiples */
  if (*pos & 3)
    return -EPROTO;

  reg = bcdev->bdev->bar0 + *pos;
  if (!reg) {
    pr_err("Invalid reg of bdma device: bar0: %p, reg:%p\n", bcdev->bdev->bar0,
           reg);
    return -EINVAL;
  }

  rv = copy_from_user(&w, buf, 4);
  if (rv)
    pr_err("copy from user failed %d/4, but continuing.\n", rv);

  pr_info("%s(0x%08x @%p, count=%ld, pos=%d)\n", __func__, w, reg, (long)count,
          (int)*pos);

  iowrite32(w, reg);
  *pos += 4;
  return 4;
}

static ssize_t engine_read(struct file *file, char __user *buf, size_t count,
                           loff_t *pos) {
  struct bdma_cdev *bcdev = (struct bdma_cdev *)file->private_data;
  struct bdma_engine *engine = bcdev->engine;
  struct sw_desc desc = {0, 0, 0};
  size_t copy_size;

  if (engine->got_registerd_region) {
    desc.addr = engine->region.head_page;
    desc.length = engine->region.tail_page - engine->region.head_page;
  };

  copy_size = min(sizeof(desc), count);
  if (copy_to_user(buf, &desc, copy_size)) {
    pr_err("Could not read registered memory from engine!\n");
    return -EFAULT;
  }
  return copy_size;
};

static ssize_t engine_write(struct file *file, const char __user *buf,
                            size_t count, loff_t *pos) {
  struct bdma_cdev *bcdev = (struct bdma_cdev *)file->private_data;
  struct bdma_engine *engine = bcdev->engine;
  struct sw_desc desc;
  int rv = 0;

  if (count < sizeof(desc)) {
    pr_err("Wrong format for engine cdev!\n");
    return -EINVAL;
  }

  if (copy_from_user(&desc, buf, sizeof(desc))) {
    pr_err("Could not copy desc from user to engine!\n");
    return -EFAULT;
  }

  switch (desc.control) {
  case BDMA_ENGINE_READ:
  case BDMA_ENGINE_WRITE:
    rv = submit_transfer(desc.addr, desc.length, desc.control, engine);
    if (rv)
      return rv;
    break;
  case BDMA_ENGINE_MR:
    rv = memory_register(desc.addr, desc.length, engine);
    if (rv)
      return rv;
    break;
  default:
    return -EINVAL;
  }

  return count;
};

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

static loff_t char_llseek(struct file *file, loff_t offset, int whence) {
  loff_t new_pos;

  switch (whence) {
  case SEEK_SET:
    new_pos = offset;
    break;
  case SEEK_CUR:
    new_pos = file->f_pos + offset;
    break;
  default:
    return -EINVAL;
  }

  if (new_pos < 0 || new_pos & 3)
    return -EINVAL;

  file->f_pos = new_pos;
  return new_pos;
}

static const struct file_operations ctrl_fops = {.owner = THIS_MODULE,
                                                 .open = char_open,
                                                 .release = char_close,
                                                 .read = ctrl_read,
                                                 .write = ctrl_write,
                                                 .llseek = char_llseek};

static const struct file_operations engine_fops = {.owner = THIS_MODULE,
                                                   .open = char_open,
                                                   .release = char_close,
                                                   .read = engine_read,
                                                   .write = engine_write,
                                                   .llseek = char_llseek};

// Only support ctrl cdev now
// bdma_control is a raw interface for registers visiting
static int create_bcdev(struct bdma_dev *bdev, struct bdma_cdev *bcdev, int bar,
                        enum cdev_type type, struct bdma_engine *engine,
                        struct class *class) {
  int rv = 0;
  int minor;
  int idx = 0;
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
  bcdev->engine = engine;

  switch (type) {
  case CHAR_CONTROL:
    minor = type;
    cdev_init(&bcdev->cdev, &ctrl_fops);
    idx = 0;
    break;
  case CHAR_ENGINE:
    minor = type + engine->channel;
    cdev_init(&bcdev->cdev, &engine_fops);
    idx = engine->channel;
    break;
  default:
    pr_err("Could not solve type %d char device.\n", type);
    return -EINVAL;
  }

  bcdev->cdevno = MKDEV(bdev->major, minor);

  rv = cdev_add(&bcdev->cdev, bcdev->cdevno, 1);
  if (rv) {
    pr_err("cdev add failed %d\n", rv);
    goto unregister_region;
  }

  bcdev->sys_device = device_create(class, &bdev->pdev->dev, bcdev->cdevno,
                                    NULL, devnode_names[type], idx);
  if (!bcdev->sys_device) {
    pr_err("device_create %s failed!\n", devnode_names[type]);
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
  int eg_idx;

  g_bdma_class = class_create(THIS_MODULE, BDMA_NODE_NAME);
  if (IS_ERR(g_bdma_class)) {
    rv = PTR_ERR(g_bdma_class);
    pr_err("Failed to create class.\n");
  };

  rv =
      create_bcdev(bdev, &bdev->ctrl_cdev, 0, CHAR_CONTROL, NULL, g_bdma_class);
  if (rv) {
    pr_err("create control char file failed %d\n", rv);
    return rv;
  }

  for (eg_idx = 0; eg_idx < bdev->c2h_channel_num; eg_idx++) {
    rv = create_bcdev(bdev, &bdev->engine_cdev[eg_idx], 0, CHAR_ENGINE,
                      &bdev->engines[eg_idx], g_bdma_class);
    if (rv) {
      pr_err("create engine char file failed %d\n", rv);
      return rv;
    }
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
