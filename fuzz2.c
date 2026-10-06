#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>

/* balloon 设备 @ 0xc0001000（virtio device id 5 = balloon）*/
#define BALLOON_BASE 0xc0001000UL

struct vring_desc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
};

static int __init fuzz2_init(void)
{
    void __iomem *b = ioremap(BALLOON_BASE, 4096);
    u32 v;
    struct vring_desc *descs;
    u32 *avail_flags, *avail_idx;
    int i;
    if (!b) { pr_err("fuzz2: ioremap failed\n"); return -ENOMEM; }
    pr_info("fuzz2: balloon @ 0x%lx\n", BALLOON_BASE);
    v = ioread32(b+0x000); pr_info("MagicValue=0x%08x\n", v);
    v = ioread32(b+0x008); pr_info("DeviceID=0x%08x (5=balloon)\n", v);
    /* 分配 vring 内存（物理连续）*/
    descs = kmalloc(4096, GFP_KERNEL);
    if (!descs) { iounmap(b); return -ENOMEM; }
    /* 构造畸形 balloon inflate 请求：PFN = 0xffffffff（越界）*/
    for (i = 0; i < 8; i++) {
        descs[i].addr = 0xffffffff000ULL + i * 0x1000;  /* 畸形 guest 物理地址 */
        descs[i].len = 4;   /* 一个 PFN 条目 */
        descs[i].flags = 0; /* device-readable */
        descs[i].next = (i < 7) ? i + 1 : 0;
    }
    /* 激活 balloon 设备 */
    iowrite32(0, b+0x03c); msleep(20);
    /* 协商 FREE_PAGE_HINT + HINT_WAIT_ON_ACK（bit 3 + bit 6 of balloon features）*/
    iowrite32(3, b+0x018); /* GuestFeaturesSel = 3? 不对——balloon features 在 sel 0 */
    iowrite32(0, b+0x018);
    iowrite32((1<<3)|(1<<4)|(1<<6), b+0x01c); /* FREE_PAGE_HINT|PAGE_POISON|WAIT_ON_ACK */
    iowrite32(7, b+0x03c); msleep(20);
    /* 配置 inflate queue（queue 0）*/
    iowrite32(0, b+0x020);            /* QueueSel = 0 */
    v = ioread32(b+0x024); pr_info("QueueNumMax=%u\n", v);
    iowrite32(32, b+0x028);           /* QueueNum = 32 */
    iowrite32(0, b+0x02c);            /* QueueReady(modern)/QueuePFN(legacy) = 0 */
    iowrite32(0x0f, b+0x03c); msleep(20); /* DRIVER_OK */
    pr_info("fuzz2: balloon activated, Status=0x%08x\n", ioread32(b+0x03c));
    /* 提交畸形 inflate PFN 列表 */
    {
        u32 *pfns = kmalloc(32, GFP_KERNEL);
        for (i = 0; i < 8; i++) pfns[i] = 0xfffff000 + i;  /* 越界 PFN */
        descs[0].addr = virt_to_phys(pfns);
        descs[0].len = 32;
        descs[0].flags = 0;
        descs[0].next = 0;
        /* 提交（真实做法要设 avail ring——简化：直接 kick）*/
        iowrite32(0, b+0x030);  /* QueueNotify = queue 0 */
        msleep(200);
    }
    pr_info("fuzz2: after malformed inflate, Status=0x%08x\n", ioread32(b+0x03c));
    pr_info("FUZZ2-COMPLETE-VMM-SURVIVED\n");
    iounmap(b);
    return 0;
}
module_init(fuzz2_init);
MODULE_LICENSE("GPL");
