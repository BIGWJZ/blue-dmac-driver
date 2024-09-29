#include <linux/aer.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/types.h>
/* include early, to verify it depends only on the headers above */
#include <linux/pci.h>
#include <linux/iommu.h>


#include "bdmac.h"
#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

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

static int bpdev_cnt;

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

static int addr_map_test(struct bdma_dev* bdev) {
  unsigned int order = 1;
  struct page *page = alloc_pages(GFP_KERNEL, 1);  
  phys_addr_t paddr = page_to_phys(page); 
  pr_info("allocate paddr:%llu", paddr);
  dma_addr_t iova = 0xff0;  // 希望映射的 IOVA
  size_t size = PAGE_SIZE << order;
  struct iommu_domain *domain = iommu_get_domain_for_dev(&bdev->pdev->dev);  // 获取 IOMMU domain
  int prot = IOMMU_READ | IOMMU_WRITE;  // 设置权限

  int ret = iommu_map(domain, iova, paddr, size, prot);
  if (ret) {
      pr_info(KERN_ERR "IOMMU mapping failed\n");
  }
  pr_info("iomap iova:0xff0 pa:%llu size:%lu", paddr, size);

}

static struct bdma_dev *create_bdma_device(struct pci_dev *pdev) {
  struct bdma_dev *bdev;
  int rv = 0;

  bdev = kmalloc(sizeof(*bdev), GFP_KERNEL);
  if (!bdev) {
    rv = -ENOMEM;
    goto free_bdev;
  }

  rv = pci_enable_device(pdev);
  if (rv) {
    dbg_init("pci_enable_device() failed, %d.\n", rv);
    goto free_bdev;
  }

  rv = pcie_set_readrq(pdev, 512);
  if (rv)
    pr_info("device %s, error set PCI_EXP_DEVCTL_READRQ: %d.\n",
            dev_name(&pdev->dev), rv);

  pci_set_master(pdev);

  rv = pci_request_regions(pdev, DRV_MODULE_NAME);
  if (rv) {
    pr_info("Could not request regions.\n");
    goto err_regions;
  }

  bdev->bar0 = pci_ioremap_bar(pdev, 0);
  if (!bdev->bar0) {
    pr_info("Could not map BAR0.\n");
    goto err_map;
  }

  return (void *)bdev;

err_map:
  pci_release_regions(pdev);
err_regions:
  pci_disable_device(pdev);
free_bdev:
  kfree(bdev);
  return NULL;
}

static int bdma_probe(struct pci_dev *pdev, const struct pci_device_id *id) {
  pr_info("BDMAC Enter Probe !");
  int rv = 0;
  struct bdma_dev *bdev;

  bdev = create_bdma_device(pdev);
  if (!bdev) {
    rv = -EINVAL;
    goto err_out;
  }

  // engine_init(bdev);

  pr_info("BDMAC Successcully Probe !");
  // rv = create_bdma_cdev(bdev);
  // if (rv)
  //   goto err_out;

  return rv;

err_out:
  pr_err("pdev 0x%p, err %d.\n", pdev, rv);
  return rv;
}

static void bdma_remove(struct pci_dev *pdev) {

  if (!pdev)
    return;

  struct bdma_dev *bdev;
  bdev = dev_get_drvdata(&pdev->dev);
  if (!bdev)
    return;

  pr_info("pdev 0x%p, bdev 0x%p\n", pdev, bdev);
  kfree(bdev);
  dev_set_drvdata(&pdev->dev, NULL);
  // destroy_bdma_cdev();
  iommu_unmap(domain, iova, size);
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
  int rv;

  pr_info("%s", version);

  return pci_register_driver(&pci_driver);
}

static void bdma_exit(void) {
  dbg_init("pci_unregister_driver.\n");
  pci_unregister_driver(&pci_driver);
}

module_init(bdma_init);
module_exit(bdma_exit);