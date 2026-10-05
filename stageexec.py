import ctypes, os, subprocess, time
libc = ctypes.CDLL(None, use_errno=True)
libc.syscall.restype = ctypes.c_long
def try_load(kf, ifd, cmd, flags):
    buf = ctypes.create_string_buffer(cmd, max(len(cmd),1)+1)
    ctypes.set_errno(0)
    r = libc.syscall(320, ctypes.c_int(kf), ctypes.c_int(ifd), ctypes.c_ulong(len(cmd)+1), buf, ctypes.c_ulong(flags))
    return r, ctypes.get_errno()
k = os.open("/boot/vmlinuz-6.12.111+deb13-cloud-amd64", os.O_RDONLY)
i = os.open("/boot/initrd.img-6.12.111+deb13-cloud-amd64", os.O_RDONLY)
cmd = b"console=ttyS0 root=/dev/vda rw panic=-1 ip=169.254.0.21::169.254.0.22:255.255.255.252:instance:eth0:off"
print("unload:", try_load(-1,-1,b"",1))
r,e = try_load(k, i, cmd, 0)
print("LOAD:", r, e)
os.close(k); os.close(i)
if r != 0:
    print("STAGE-FAILED — aborting")
    raise SystemExit(1)
print("STAGED-OK — switching in 3s")
open("/home/user/SWITCH-MARKER", "w").write("kexec-switch")
os.sync()
time.sleep(3)
# 直接用 reboot syscall 触发 kexec（不依赖 kexec-tools 二进制）
LINUX_REBOOT_MAGIC1 = 0xfee1dead
LINUX_REBOOT_MAGIC2 = 672274793
LINUX_REBOOT_CMD_KEXEC = 0x45584543
libc.reboot.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_void_p]
rc = libc.reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_KEXEC, None)
print("reboot(kexec) returned:", rc, "errno:", ctypes.get_errno())
