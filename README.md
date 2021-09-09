[![Build status](https://travis-ci.org/mpromonet/v4l2rtspclient.png)](https://travis-ci.org/mpromonet/v4l2rtspclient)
[![CirusCI](https://api.cirrus-ci.com/github/mpromonet/v4l2rtspclient.svg?branch=master)](https://cirrus-ci.com/github/mpromonet/v4l2rtspclient)

[![Docker Pulls](https://img.shields.io/docker/pulls/mpromonet/v4l2rtspclient.svg)](https://hub.docker.com/r/mpromonet/v4l2rtspclient)

v4l2rtspclient
==============
This is an RTSP client that write to a V4L2 output device.
It support only H264 format.

Dependencies
------------
 - [libv4l2cpp](https://github.com/mpromonet/libv4l2cpp) as a submodule
 - [live555helper](https://github.com/mpromonet/live555helper) as a submodule
 - [h264bitstream](https://github.com/aizvorski/h264bitstream) as a submodule

Build
------- 
	make

