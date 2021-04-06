Wipe the free disk space on Linux.
Inspired by zerofree at http://intgat.tigress.co.uk/rmy/uml/sparsify.html

Root required.
No need to unmount - uses OS API: fallocate (2) and ioctl_list (2) FIBMAP.

Tested with Ext4, Kernel 3.17, glibc 2.20
see fallocate(2) for supported file systems

Note: on VirtualBox .vdi container `dd if=/dev/zero` will perform better than this program, because VBox handles all-zero writes in a smart way.
