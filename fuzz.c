#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>

#define NET_BASE 0xc0003000UL

static int __init fuzz_init(void)
{
    void __iomem *b = ioremap(NET_BASE, 4096);
    u32 v; int i;
    if (!b) { pr_err("fuzz: ioremap failed\n"); return -ENOMEM; }
    pr_info("fuzz: mapped virtio-mmio net @ phys 0x%lx\n", NET_BASE);
    v = ioread32(b+0x000); pr_info("MagicValue=0x%08x\n", v);
    v = ioread32(b+0x004); pr_info("Version=0x%08x\n", v);
    v = ioread32(b+0x008); pr_info("DeviceID=0x%08x\n", v);
    v = ioread32(b+0x024); pr_info("QueueNumMax=%u\n", v);
    v = ioread32(b+0x03c); pr_info("Status=0x%08x\n", v);
    iowrite32(0, b+0x03c); msleep(30);
    iowrite32(0x0f, b+0x03c); msleep(30);
    pr_info("post-activate Status=0x%08x\n", ioread32(b+0x03c));
    for (i = 0; i < 6; i++) {
        iowrite32((u32)(i*0x33333333), b+0x020);
        iowrite32(0, b+0x028);
        iowrite32(0xffffffffu, b+0x028);
        iowrite32(0x7ffff000u, b+0x02c);
        msleep(15);
    }
    iowrite32(0x0fffffffu, b+0x030);
    msleep(40);
    pr_info("fuzz: still alive, Status=0x%08x\n", ioread32(b+0x03c));
    pr_info("FUZZ-COMPLETE-VMM-SURVIVED\n");
    iounmap(b);
    return 0;
}
module_init(fuzz_init);
MODULE_LICENSE("GPL");
