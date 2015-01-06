TARGET = livezerofree
OFILES = logging.o livezerofree.o df.o
MPOINT = /media/testlivezerofree
IMGFILE = test.bin
IMGLINK = $(MPOINT)/x/$(IMGFILE)
GARBAGEFILE = garbage.bin
MKFS = mkfs.ext4

$(TARGET): $(OFILES)

*.o: mylastheader.h mylogging.h mydf.h

cleanall: clean
	@sudo $(MAKE) cleanroot

clean:
	rm -f *.o

test: $(TARGET)
	@sudo $(MAKE) testroot

cleanroot:
	rm -f $(MPOINT)/x
	! mount | grep $(MPOINT) >/dev/null || umount $(MPOINT)
	! a=`losetup -l | grep $(IMGFILE)` || for dev in $$a; do losetup -d $$dev; break; done
	rm -f $(IMGFILE) $(GARBAGEFILE)

testpre: $(IMGLINK) $(GARBAGEFILE)
	rm -f $(MPOINT)/bigfile
	dd bs=8192 if=$(GARBAGEFILE) of=$(MPOINT)/garbage
	dd bs=8192 if=/dev/zero of=$(MPOINT)/zero || true
	rm -f $(MPOINT)/garbage $(MPOINT)/zero

testroot: $(IMGLINK) $(GARBAGEFILE) testpre
	sync
	echo 1 > /proc/sys/vm/drop_caches
	./livezerofree $(MPOINT)/bigfile
	sync
	echo 1 > /proc/sys/vm/drop_caches
	./livezerofree $(MPOINT)/bigfile

$(IMGLINK): $(MPOINT) $(IMGFILE)
	mount | grep $(MPOINT) >/dev/null || mount -oloop $(IMGFILE) $(MPOINT)
	chmod 777 $(MPOINT)
	rm -f $(MPOINT)/x
	ln -sf $(CURDIR) $(MPOINT)/x
	touch $(IMGFILE)

$(GARBAGEFILE):
	dd if=/dev/urandom of=$(GARBAGEFILE).tmp bs=8192 count=6000
	mv $(GARBAGEFILE).tmp $(GARBAGEFILE)

$(IMGFILE):
	dd if=/dev/null of=$(IMGFILE).tmp count=0 bs=8192 seek=12000
	$(MKFS) $(IMGFILE).tmp
	mv $(IMGFILE).tmp $(IMGFILE)

$(MPOINT):
	mkdir -p $(MPOINT)

