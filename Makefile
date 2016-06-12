CFLAGS = -W -Wall -pthread -g -pipe $(CFLAGS_EXTRA) -I include
RM = rm -rf
CC = g++

# log4cpp
LDFLAGS += -llog4cpp 
# libv4l2cpp
CFLAGS += -I libv4l2cpp/inc
# live555
CFLAGS += -I $(SYSROOT)/usr/include/liveMedia  -I $(SYSROOT)/usr/include/groupsock -I $(SYSROOT)/usr/include/UsageEnvironment -I $(SYSROOT)/usr/include/BasicUsageEnvironment/
LDFLAGS += -lliveMedia -lgroupsock -lUsageEnvironment -lBasicUsageEnvironment
# h264bitstream
CFLAGS += -I h264bitstream



v4l2rtspclient: h264bitstream/h264_stream.c h264bitstream/h264_nal.c h264bitstream/h264_sei.c src/v4l2rtspclient.cpp libv4l2cpp.a
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

h264bitstream/h264_stream.c:
	git submodule init h264bitstream
	git submodule update h264bitstream

libv4l2cpp.a: 
	git submodule init libv4l2cpp
	git submodule update libv4l2cpp
	make -C libv4l2cpp
	mv libv4l2cpp/libv4l2wrapper.a $@ 
	make -C libv4l2cpp clean

upgrade:
	git submodule foreach git pull origin master
	
install:
	install -m 0755 $(ALL_PROGS) /usr/local/bin

clean:
	-@$(RM) v4l2rtspclient .*o *.a

