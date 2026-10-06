import ctypes, os, time
libc = ctypes.CDLL(None, use_errno=True)
libc.syscall.restype = ctypes.c_long
def try_load(kf, ifd, cmd, flags):
    buf = ctypes.create_string_buffer(cmd, max(len(cmd),1)+1)
    ctypes.set_errno(0)
    r = libc.syscall(320, ctypes.c_int(kf), ctypes.c_int(ifd), ctypes.c_ulong(len(cmd)+1), buf, ctypes.c_ulong(flags))
    return r, ctypes.get_errno()
k = os.open("/boot/vmlinuz-6.12.111+deb13-cloud-amd64", os.O_RDONLY)
i = os.open("/boot/initrd.img-6.12.111+deb13-cloud-amd64", os.O_RDONLY)
# 完整 cmdline：保留原 boot 的全部关键参数 + virtio_mmio 设备映射
cmd = (b"console=ttyS0 root=/dev/vda rw panic=-1 "
       b"ip=169.254.0.21::169.254.0.22:255.255.255.252:instance:eth0:off "
       b"clocksource=kvm-clock pci=off reboot=k rootflags=discard "
       b"virtio_mmio.device=4K@0xc0001000:6 "
       b"virtio_mmio.device=4K@0xc0002000:7 "
       b"virtio_mmio.device=4K@0xc0003000:8 "
       b"virtio_mmio.device=4K@0xc0004000:9")
print("unload:", try_load(-1,-1,b"",1))
r,e = try_load(k, i, cmd, 0)
print("LOAD:", r, e)
os.close(k); os.close(i)
if r != 0:
    print("STAGE-FAILED")
    raise SystemExit(1)
print("STAGED-OK")
open("/home/user/SWITCH-MARKER", "w").write("kexec-switch")
os.sync()
time.sleep(3)
LINUX_REBOOT_MAGIC1 = 0xfee1dead
LINUX_REBOOT_MAGIC2 = 672274793
LINUX_REBOOT_CMD_KEXEC = 0x45584543
libc.reboot.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
rc = libc.reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_KEXEC, None)
print("reboot(kexec) returned:", rc, "errno:", ctypes.get_errno())
