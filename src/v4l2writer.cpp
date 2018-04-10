/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2writer.cpp
** 
** Implement RTSP Client connection interface writing to a V4L2 output device
** 
** -------------------------------------------------------------------------*/


#include "logger.h"

#include "v4l2writer.h"

uint8_t marker[] = { 0, 0, 0, 1};

V4l2Writer::V4l2Writer(const std::string &out_devname, V4l2Access::IoType ioTypeOut) : m_videoOutput(NULL), m_out_devname(out_devname), m_ioTypeOut(ioTypeOut) 
{
	m_h264 = h264_new();
}
		
V4l2Writer::~V4l2Writer()
{
	delete m_videoOutput;
	h264_free(m_h264);
}

ssize_t V4l2Writer::onNewBuffer(unsigned char* buffer, ssize_t size)
{
	ssize_t markerSize = 0;
	if (size > sizeof(marker))
	{
		memcpy( buffer, marker, sizeof(marker) );
		markerSize = sizeof(marker);
	}
	return 	markerSize;		
}

bool V4l2Writer::onNewSession(const char* id, const char* media, const char* codec)
{
	bool success = false;
	if ( (strcmp(media, "video") == 0) && (strcmp(codec, "H264") == 0) )
	{
		success = true;
	}
	return success;
}

bool V4l2Writer::onData(const char* id, unsigned char* buffer, ssize_t size, timeval tv)
{
	bool success = false;

	if (m_videoOutput == NULL)
	{			
		int nal_start = 0;
		int nal_end   = 0;
		find_nal_unit(buffer, size, &nal_start, &nal_end);
		read_nal_unit(m_h264, &buffer[nal_start], nal_end - nal_start);
		LOG(DEBUG) << "NAL:" << m_h264->nal->nal_unit_type;
		if (m_h264->nal->nal_unit_type == NAL_UNIT_TYPE_SPS)
		{
			unsigned int width = ((m_h264->sps->pic_width_in_mbs_minus1 +1)*16) - m_h264->sps->frame_crop_left_offset*2 - m_h264->sps->frame_crop_right_offset*2;
			unsigned int height= ((2 - m_h264->sps->frame_mbs_only_flag)* (m_h264->sps->pic_height_in_map_units_minus1 +1) * 16) - (m_h264->sps->frame_crop_top_offset * 2) - (m_h264->sps->frame_crop_bottom_offset * 2);
			
			LOG(NOTICE) << "geometry:" << width << "x" << height;
			
			V4L2DeviceParameters outparam(m_out_devname.c_str(), V4L2_PIX_FMT_H264, width, height, 0, 255);
			m_videoOutput = V4l2Output::create(outparam, m_ioTypeOut);			
			success = (m_videoOutput != NULL);					
		}
	}
	
	if (m_videoOutput)
	{
		size_t ret = m_videoOutput->write((char*)buffer, size);
		success = (ret == size);					
	}
	return success;
}


