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
CFLAGS += -I live555helper/live/liveMedia/include  -I live555helper/live/groupsock/include -I live555helper/live/UsageEnvironment/include -I live555helper/live/BasicUsageEnvironment/include
#LDFLAGS += -L live555helper/live -lliveMedia -lgroupsock -lUsageEnvironment -lBasicUsageEnvironment
# live555helper
CFLAGS += -I live555helper/inc -DNO_OPENSSL=1
# h264bitstream
CFLAGS += -I h264bitstream



v4l2rtspclient: h264bitstream/h264_stream.c h264bitstream/h264_nal.c h264bitstream/h264_sei.c src/v4l2rtspclient.cpp src/v4l2writer.cpp libv4l2cpp.a libliblive555helper.a
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

libliblive555helper.a:
	git submodule init live555helper
	git submodule update live555helper
	cd live555helper && cmake . && make && cd - 
	mv live555helper/$@ $@ 
	make -C live555helper clean

upgrade: 
	git submodule foreach git pull origin master
	
install: v4l2rtspclient
	mkdir -p $(PREFIX)/bin
	install -m 0755 v4l2rtspclient $(PREFIX)/bin

clean:
	-@$(RM) v4l2rtspclient .*o *.a

