#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

#include "libbdma.h"

// int addr_map_test(struct bdma_dev *bdev) {
//   int rv = 0;
//   unsigned int order = 1;
//   struct page *page = alloc_pages(GFP_KERNEL, 1);
//   phys_addr_t paddr = page_to_phys(page);
//   pr_info("allocate paddr:%x\n", paddr);

//   dma_addr_t iova = 0xff0;
//   dma_addr_t dma_handle;
//   void *pa;
//   struct device *dev = &bdev->pdev->dev;
//   pa = dma_alloc_coherent(dev, 256, &dma_handle, GFP_KERNEL);

//   pr_info("dma_alloc_coherenct: dma_addr:%llx, qemu_pa:%llx\n", dma_handle,
//   pa); return rv;
// }

static int set_dma_mask(struct pci_dev *pdev) {
  if (!pdev) {
    pr_err("Invalid pdev in set_dma_mask\n");
    return -EINVAL;
  }

  if (!dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64))) {

  } else if (!dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32))) {

  } else {
    return -EINVAL;
  }
  return 0;
}

struct bdma_dev *create_bdma_device(const char *mod_name,
                                    struct pci_dev *pdev) {
  struct bdma_dev *bdev;
  int rv = 0;

  bdev = kmalloc(sizeof(*bdev), GFP_KERNEL);
  if (!bdev) {
    rv = -ENOMEM;
    goto free_bdev;
  }

  bdev->pdev = pdev;
  bdev->mod_name = mod_name;

  rv = pci_enable_device(pdev);
  if (rv) {
    pr_err("pci_enable_device() failed, %d.\n", rv);
    goto free_bdev;
  }

  rv = pcie_set_readrq(pdev, 512);
  if (rv)
    pr_err("device %s, error set PCI_EXP_DEVCTL_READRQ: %d.\n",
           dev_name(&pdev->dev), rv);

  pci_set_master(pdev);

  rv = pci_request_regions(pdev, bdev->mod_name);
  if (rv) {
    pr_err("Could not request regions %d.\n", rv);
    bdev->regions_in_use = 1;
    goto err_regions;
  } else {
    bdev->got_regions = 1;
  }

  bdev->bar0 = pci_ioremap_bar(pdev, 0);
  if (!bdev->bar0) {
    pr_err("Could not map BAR0.\n");
    goto err_map;
  }

  rv = set_dma_mask(pdev);
  if (rv) {
    pr_err("Set mask error!\n");
    goto err_mask;
  }

  pr_info("bdma device created successfully, bar0%p, bdev%p, pdev%p",
          bdev->bar0, bdev, pdev);

  return (void *)bdev;

err_mask:
  pci_iounmap(pdev, bdev->bar0);
err_map:
  pci_release_regions(pdev);
err_regions:
  pci_disable_device(pdev);
free_bdev:
  kfree(bdev);
  return NULL;
}

void remove_bdma_device(struct pci_dev *pdev, void *dev_hndl) {
  struct bdma_dev *bdev = (struct bdma_dev *)dev_hndl;

  if (!dev_hndl)
    return;

  if (bdev->pdev != pdev) {
    pr_err("Mismatch of bdma close\n");
  }

  // remove_engines;

  pci_iounmap(pdev, bdev->bar0);
  bdev->bar0 = NULL;

  if (bdev->got_regions) {
    pci_release_regions(pdev);
  }

  if (!bdev->regions_in_use) {
    pci_disable_device(pdev);
  }

  kfree(bdev);
  pr_info("remove bdma device, pdev 0x%p, bdev 0x%p\n", pdev, bdev);
}

// TODO: transfers handle functions
