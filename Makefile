CFLAGS = -W -Wall -pthread -g -pipe $(CFLAGS_EXTRA) -I inc
RM = rm -rf
CC = $(CROSS)gcc
CXX = $(CROSS)g++
PREFIX?=/usr


# log4cpp
ifneq ($(wildcard $(SYSROOT)$(PREFIX)/include/log4cpp/Category.hh),)
$(info with log4cpp)
CFLAGS += -DHAVE_LOG4CPP -I $(SYSROOT)$(PREFIX)/include
LDFLAGS += -llog4cpp
endif

# libv4l2cpp
CFLAGS += -I libv4l2cpp/inc
# live555
CFLAGS += -I $(SYSROOT)/usr/include/liveMedia  -I $(SYSROOT)/usr/include/groupsock -I $(SYSROOT)/usr/include/UsageEnvironment -I $(SYSROOT)/usr/include/BasicUsageEnvironment/
LDFLAGS += -lliveMedia -lgroupsock -lUsageEnvironment -lBasicUsageEnvironment
# live555helper
CFLAGS += -I live555helper/inc
# h264bitstream
CFLAGS += -I h264bitstream



v4l2rtspclient: h264bitstream/h264_stream.c h264bitstream/h264_nal.c h264bitstream/h264_sei.c src/v4l2rtspclient.cpp src/v4l2writer.cpp libv4l2cpp.a live555helper.a
	$(CXX) -o $@ $(CFLAGS) $^ $(LDFLAGS)

h264bitstream/h264_stream.c:
	git submodule init h264bitstream
	git submodule update h264bitstream

libv4l2cpp.a: 
	git submodule init $(*F)
	git submodule update $(*F)
	make -C $(*F)
	mv $(*F)/*.a $@ 
	make -C $(*F) clean

live555helper.a:
	git submodule init $(*F)
	git submodule update $(*F)
	make -C $(*F)
	mv $(*F)/$@ $@ 
	make -C $(*F) clean

upgrade: 
	git submodule foreach git pull origin master
	
install: v4l2rtspclient
	mkdir -p $(PREFIX)/bin
	install -m 0755 $(ALL_PROGS) $(PREFIX)/bin

clean:
	-@$(RM) v4l2rtspclient .*o *.a

