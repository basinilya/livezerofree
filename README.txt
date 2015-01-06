Wipe the free disk space on Linux.
Inspired by zerofree at http://intgat.tigress.co.uk/rmy/uml/sparsify.html

Root required.
No need to unmount - uses OS API.

Tested with Ext4, Kernel 3.17, glibc 2.20
see fallocate(2) for supported file systems

