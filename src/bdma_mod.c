#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

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

#include "bdma_cdev.h"
#include "libbdma.h"

#define DRV_MODULE_NAME "bdma"
#define DRV_MODULE_DESC "BlueDMA Driver"
#define DRV_MOD_MAJOR 2024
#define DRV_MOD_MINOR 1
#define DRV_MOD_PATCHLEVEL 1
#define DRV_MODULE_VERSION                                                     \
  __stringify(DRV_MOD_MAJOR) "." __stringify(DRV_MOD_MINOR) "." __stringify(   \
      DRV_MOD_PATCHLEVEL)

static char version[] =
    DRV_MODULE_DESC " " DRV_MODULE_NAME " v" DRV_MODULE_VERSION "\n";

MODULE_AUTHOR("Jingzhi Wang");
MODULE_DESCRIPTION(DRV_MODULE_DESC);
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_LICENSE("Dual BSD/GPL");

static const struct pci_device_id pci_ids[] = {{
                                                   PCI_DEVICE(0x10ee, 0x9048),
                                               },
                                               {
                                                   PCI_DEVICE(0x10ee, 0x9044),
                                               },
                                               {
                                                   PCI_DEVICE(0x10ee, 0x9042),
                                               },
                                               {
                                                   PCI_DEVICE(0x10ee, 0x9041),
                                               },
                                               {
                                                   PCI_DEVICE(0x10ee, 0x903f),
                                               },
                                               {0}};
MODULE_DEVICE_TABLE(pci, pci_ids);

static int bdma_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
  int rv = 0;
  struct bdma_dev *bdev;

  bdev = create_bdma_device(DRV_MODULE_NAME, pdev);
  if (!bdev) {
    rv = -EINVAL;
    goto err_out;
  }

  // TODO: allocate poll_result_va, write to hw
  // engine_init(bdev);

  rv = bdev_create_interfaces(bdev);
  if (rv)
    goto err_out;

  return rv;

err_out:
  pr_info("pdev 0x%p, err %d.\n", pdev, rv);
  return rv;
}

static void bdma_remove(struct pci_dev *pdev) {
  struct bdma_dev *bdev;

  if (!pdev)
    return;

  bdev = dev_get_drvdata(&pdev->dev);
  if (!bdev)
    return;

  remove_bdma_device(pdev, bdev);
  dev_set_drvdata(&pdev->dev, NULL);

  bdev_destroy_interfaces(bdev);
}

// TODO
// static const struct pci_error_handlers bdma_err_handler = {
//     .error_detected = bdma_error_detected,
//     .slot_reset = bdma_slot_reset,
//     .resume = bdma_error_resume,
//     .reset_prepare = bdma_reset_prepare,
//     .reset_done = bdma_reset_done};

static struct pci_driver pci_driver = {.name = DRV_MODULE_NAME,
                                       .id_table = pci_ids,
                                       .probe = bdma_probe,
                                       .remove = bdma_remove,
                                       .err_handler = NULL};

static int bdma_init(void) {
  pr_info("%s", version);
  return pci_register_driver(&pci_driver);
}

static void bdma_exit(void) {
  pr_info("bdma unregister.\n");
  pci_unregister_driver(&pci_driver);
}

module_init(bdma_init);
module_exit(bdma_exit);