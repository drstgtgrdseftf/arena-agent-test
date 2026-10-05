import ctypes, os
libc = ctypes.CDLL(None, use_errno=True)
libc.syscall.restype = ctypes.c_long
def try_load(kf, ifd, cmd, flags):
    buf = ctypes.create_string_buffer(cmd, max(len(cmd),1)+1)
    ctypes.set_errno(0)
    r = libc.syscall(320, ctypes.c_int(kf), ctypes.c_int(ifd), ctypes.c_ulong(len(cmd)+1), buf, ctypes.c_ulong(flags))
    return r, ctypes.get_errno()
def loaded():
    return open("/sys/kernel/kexec_loaded").read().strip()
k = os.open("/boot/vmlinuz-6.12.111+deb13-cloud-amd64", os.O_RDONLY)
i = os.open("/boot/initrd.img-6.12.111+deb13-cloud-amd64", os.O_RDONLY)
cmd = b"console=ttyS0 root=/dev/vda rw panic=-1 ip=169.254.0.21::169.254.0.22:255.255.255.252:instance:eth0:off"
print("unload:", try_load(-1,-1,b"",1))
r,e = try_load(k, i, cmd, 0)
print("LOAD:", r, e, "| loaded:", loaded())
os.close(k); os.close(i)
print("STAGED-OK" if r==0 else "STAGE-FAILED")
