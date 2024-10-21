#include "asm-generic/iomap.h"
#include "asm/page_types.h"
#include "linux/dma-direction.h"
#include "linux/dma-mapping.h"
#include "linux/mm.h"
#include "linux/slab.h"
#include "linux/types.h"
#define pr_fmt(fmt) KBUILD_MODNAME ":%s: " fmt, __func__

#include "libbdma.h"

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

static void engine_regs_init(struct bdma_engine *engine) {
  u32 w;

  iowrite32(0, &engine->regs->desc_va_lo);
  iowrite32(0, &engine->regs->desc_va_hi);
  iowrite32(0, &engine->regs->desc_bytes);

  w = cpu_to_le32(PCI_DMA_L(engine->poll_bus_addr));
  iowrite32(w, &engine->regs->desc_va_lo);

  w = cpu_to_le32(PCI_DMA_H(engine->poll_bus_addr));
  iowrite32(w, &engine->regs->desc_va_hi);
};

static int engine_init(struct bdma_dev *bdev, int channel) {
  struct engine_regs *regs;
  struct bdma_engine *engine;
  u8 *pa_list;

  engine = &bdev->engines[channel];
  regs = bdev->bar0 + BDMA_CHANNEL_REG_BYTES * channel;
  pa_list = (u8 *)bdev->bar0 +
            (channel * BDMA_MAX_MR_PAGE_NUM * 2 + BDMA_REG_REGION) * 4;

  engine->regs = regs;
  engine->pa_list = pa_list;
  engine->id = channel;
  engine->channel = channel;
  engine->bdev = bdev;
  engine->got_registerd_region = 0;

  engine->poll_virt_addr =
      dma_alloc_coherent(&bdev->pdev->dev, sizeof(struct bdma_poll_wb),
                         &engine->poll_bus_addr, GFP_KERNEL);
  if (!engine->poll_virt_addr) {
    pr_err("Counld not alloc space for poll mode wb!\n");
    return -ENOMEM;
  }

  engine_regs_init(engine);

  return 0;
};

static void engine_remove(struct bdma_dev *bdev) {
  int idx;
  struct bdma_engine *engine;

  for (idx = 0; idx < bdev->c2h_channel_num; idx++) {
    engine = &bdev->engines[idx];
    if (engine->poll_virt_addr) {
      dma_free_coherent(&engine->bdev->pdev->dev, sizeof(struct bdma_poll_wb),
                        engine->poll_virt_addr, engine->poll_bus_addr);
    }
  }
}

int engines_probe(struct bdma_dev *bdev) {
  int rv = 0;
  int idx = 0;

  if (!bdev)
    return -EINVAL;

  for (idx = 0; idx < bdev->c2h_channel_num; idx++) {
    rv = engine_init(bdev, idx);
  };

  return rv;
};

static void write_pages(struct bdma_engine *engine, int page_idx,
                        dma_addr_t phy_addr) {
  u32 w;
  unsigned int reg_idx = 0;
  pr_info("Debug: new phy_addr : %llx", phy_addr);

  w = cpu_to_le32(PCI_DMA_L(phy_addr));
  reg_idx = page_idx * 2 + 1;
  iowrite32(w, engine->pa_list + reg_idx * 4);
  pr_info("Debug: write register @ %px, low dma addr %x",
          engine->pa_list + reg_idx * 4, w);

  w = cpu_to_le32(PCI_DMA_H(phy_addr));
  reg_idx = page_idx * 2;
  iowrite32(w, engine->pa_list + reg_idx * 4);
  pr_info("Debug: write register @ %px, high dma addr %x",
          engine->pa_list + reg_idx * 4, w);
};

int memory_register(unsigned long long user_addr, size_t user_len,
                    struct bdma_engine *engine) {
  struct mem_region *mr;
  struct page **pages;
  int num_pages, pinned;
  int page_idx;
  dma_addr_t dma_hdl;

  if (!engine || user_len <= PAGE_SIZE)
    return -EINVAL;

  mr = &engine->region;
  mr->head_page = user_addr & PAGE_MASK;
  mr->tail_page = PAGE_ALIGN(user_addr + user_len);

  num_pages = (mr->tail_page - mr->head_page) / PAGE_SIZE;
  if (num_pages > BDMA_MAX_MR_PAGE_NUM) {
    pr_err("Request more pages than support in memory registion!\n");
    return -EINVAL;
  }

  pages = kmalloc_array(num_pages, sizeof(struct page *), GFP_KERNEL);
  if (!pages)
    return -ENOMEM;

  pinned = pin_user_pages(mr->head_page, num_pages, FOLL_WRITE, pages, NULL);
  if (pinned < num_pages) {
    kfree(pages);
    if (pinned > 0)
      unpin_user_pages(pages, pinned);
    pr_err("Could not pin all requested pages!\n");
    return -EFAULT;
  }

  for (page_idx = 0; page_idx < num_pages; page_idx++) {
    dma_hdl = dma_map_page(&engine->bdev->pdev->dev, pages[page_idx], 0,
                           PAGE_SIZE, DMA_BIDIRECTIONAL);
    pr_info("Debug: dma map, page @%px , dma_hdl @%llx \n", pages[page_idx],
            dma_hdl);
    write_pages(engine, page_idx, dma_hdl);
  }

  engine->got_registerd_region = 1;
  kfree(pages);
  return 0;
};

struct bdma_dev *create_bdma_device(const char *mod_name,
                                    struct pci_dev *pdev) {
  struct bdma_dev *bdev;
  int rv = 0;

  bdev = kzalloc(sizeof(*bdev), GFP_KERNEL);
  if (!bdev) {
    rv = -ENOMEM;
    goto free_bdev;
  }

  bdev->pdev = pdev;
  bdev->mod_name = mod_name;
  bdev->c2h_channel_num = BDMA_CHANNEL_NUM_MAX;

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

  rv = engines_probe(bdev);
  if (rv) {
    pr_err("Probe engines error!\n");
    goto err_mask;
  }

  pr_info("bdma device created successfully, bar0%p, bdev%p, pdev%p\n",
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
};

void remove_bdma_device(struct pci_dev *pdev, void *dev_hndl) {
  struct bdma_dev *bdev = (struct bdma_dev *)dev_hndl;
  pr_info("Enter bdma device destroy\n");
  if (!dev_hndl)
    return;

  if (bdev->pdev != pdev) {
    pr_err("Mismatch of bdma close\n");
  }

  engine_remove(bdev);

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
};

int submit_transfer(unsigned long long user_addr, size_t length, u32 control,
                    struct bdma_engine *engine) {
  u32 w;
  struct engine_regs *regs = engine->regs;
  struct mem_region *mr = &engine->region;

  if (user_addr < mr->head_page || user_addr > mr->tail_page ||
      user_addr + length > mr->tail_page) {
    pr_err("Could not build transfer violate registerd memory bound!\n");
    return -EINVAL;
  };

  if (!(control == BDMA_ENGINE_READ || control == BDMA_ENGINE_WRITE)) {
    pr_err("Could not build transfer with wrong control!\n");
    return -EINVAL;
  }

  w = cpu_to_le32(PCI_DMA_L(user_addr));
  iowrite32(w, &regs->desc_va_lo);

  w = cpu_to_le32(PCI_DMA_H(user_addr));
  iowrite32(w, &regs->desc_va_hi);

  w = cpu_to_le32(PCI_DMA_L(length));
  iowrite32(w, &regs->desc_bytes);

  iowrite32(control, &regs->doorbell);
  return 0;
};
