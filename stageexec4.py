import ctypes, os, time, subprocess
libc = ctypes.CDLL(None, use_errno=True)
libc.syscall.restype = ctypes.c_long
def try_load(kf, ifd, cmd, flags):
    buf = ctypes.create_string_buffer(cmd, max(len(cmd),1)+1)
    ctypes.set_errno(0)
    r = libc.syscall(320, ctypes.c_int(kf), ctypes.c_int(ifd), ctypes.c_ulong(len(cmd)+1), buf, ctypes.c_ulong(flags))
    return r, ctypes.get_errno()

# 1. 构造自定义 initrd（busybox 静态版 + /init 恢复脚本）
subprocess.run(["apt-get", "install", "-y", "-qq", "busybox-static"], capture_output=True)
root = "/tmp/ird"
subprocess.run(["rm", "-rf", root])
os.makedirs(root + "/bin"); os.makedirs(root + "/proc"); os.makedirs(root + "/sys")
os.makedirs(root + "/dev"); os.makedirs(root + "/home"); os.makedirs(root + "/mod")
subprocess.run(["cp", "/bin/busybox", root + "/bin/busybox"])
os.symlink("/bin/busybox", root + "/bin/sh")
# fuzz 模块（提前编译好）
import glob
kos = glob.glob("/home/user/mod/fuzz.ko") + glob.glob("/home/user/mod/*.ko")
if kos:
    subprocess.run(["cp", kos[0], root + "/mod/"])
init_script = """#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev
mount -o remount,rw /dev/vda / 2>/dev/null || mount -o remount,rw /
sleep 1
if [ -f /mod/fuzz.ko ]; then
  insmod /mod/fuzz.ko 2>/home/insmod-err.txt
  sleep 3
  dmesg > /home/KEXEC-FUZZ-LOG.txt 2>&1
  cp /home/KEXEC-FUZZ-LOG.txt /home/user/ 2>/dev/null
  sync
fi
exec switch_root /dev/vda /sbin/init
"""
open(root + "/init", "w").write(init_script)
os.chmod(root + "/init", 0o755)
# 2. 打包 cpio.gz
os.chdir(root)
subprocess.run("find . | cpio -o -H newc 2>/dev/null | gzip > /tmp/custom-initrd.gz", shell=True)
print("initrd built:", os.path.getsize("/tmp/custom-initrd.gz"), "bytes; ko included:", bool(kos))

# 3. staging with custom initrd + clean cmdline
k = os.open("/boot/vmlinuz-6.12.111+deb13-cloud-amd64", os.O_RDONLY)
i = os.open("/tmp/custom-initrd.gz", os.O_RDONLY)
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
    print("STAGE-FAILED"); raise SystemExit(1)
print("STAGED-OK")
os.sync()
time.sleep(3)
rc = libc.syscall(169, ctypes.c_int(0xfee1dead), ctypes.c_int(672274793), ctypes.c_int(0x45584543), None)
print("reboot(kexec) rc:", rc, "errno:", ctypes.get_errno())
