/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2writer.h
** 
** Implement RTSP Client connection interface writing to a V4L2 output device
** 
** -------------------------------------------------------------------------*/

#pragma once

#include "logger.h"

#include "V4l2Output.h"

#include "rtspconnectionclient.h"
#include "h264_stream.h"


class V4l2Writer : public RTSPConnection::Callback
{
	public:
		V4l2Writer(const std::string &out_devname, V4l2Access::IoType ioTypeOut = V4l2Access::IOTYPE_MMAP);		
		virtual ~V4l2Writer();
	
		virtual ssize_t onNewBuffer(unsigned char* buffer, ssize_t size);		
		virtual bool    onNewSession(const char* id, const char* media, const char* codec);
		virtual bool    onData(const char* id, unsigned char* buffer, ssize_t size, timeval tv);
			
	private:
		V4l2Output*               m_videoOutput;
		std::string               m_out_devname;
		V4l2Access::IoType        m_ioTypeOut;
		h264_stream_t*            m_h264; 
};

