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

#include <climits>

extern "C" int read_hevc_nal_unit(hevc_stream_t* h, uint8_t* buf, int size);

uint8_t marker[] = { 0, 0, 0, 1};

V4l2Writer::V4l2Writer(const std::string &out_devname, V4l2IoType ioTypeOut) : m_videoOutput(NULL), m_out_devname(out_devname), m_ioTypeOut(ioTypeOut)
{
	m_h264 = h264_new();
	m_hevc = hevc_new();
}
		
V4l2Writer::~V4l2Writer()
{
	delete m_videoOutput;
	h264_free(m_h264);
	hevc_free(m_hevc);
}

ssize_t V4l2Writer::onNewBuffer(const char* id, const char* mime, unsigned char* buffer, ssize_t size)
{
	ssize_t markerSize = 0;
	if (size > sizeof(marker))
	{
		memcpy( buffer, marker, sizeof(marker) );
		markerSize = sizeof(marker);
	}
	return 	markerSize;		
}

bool V4l2Writer::onNewSession(const char* id, const char* media, const char* codec, const char* sdp, unsigned int rtpfrequency, unsigned int channels)
{
	bool success = false;
	if (strcmp(media, "video") == 0)
	{
		if (strcmp(codec, "H264") == 0)
		{
			m_codec = codec;
			success = true;
		}
		else if (strcmp(codec, "H265") == 0)
		{
			m_codec = codec;
			success = true;
		}
	}
	if (!success)
	{
		m_codec.clear();
	}
	return success;
}

bool V4l2Writer::onData(const char* id, unsigned char* buffer, ssize_t size, timeval tv)
{
	bool success = false;

	if (m_videoOutput == NULL)
	{
		unsigned int width = 0;
		unsigned int height = 0;
		unsigned int format = 0;
		if (size > static_cast<ssize_t>(sizeof(marker)) && size <= INT_MAX)
		{
			const int nal_start = sizeof(marker);
			const int nal_size = static_cast<int>(size) - nal_start;
			if (m_codec == "H264" && nal_size > 0)
			{
				if (read_nal_unit(m_h264, &buffer[nal_start], nal_size) > 0)
				{
					LOG(DEBUG) << "NAL:" << m_h264->nal->nal_unit_type;
					if (m_h264->nal->nal_unit_type == NAL_UNIT_TYPE_SPS)
					{
						width = ((m_h264->sps->pic_width_in_mbs_minus1 +1)*16) - m_h264->sps->frame_crop_left_offset*2 - m_h264->sps->frame_crop_right_offset*2;
						height = ((2 - m_h264->sps->frame_mbs_only_flag) * (m_h264->sps->pic_height_in_map_units_minus1 +1) * 16) - (m_h264->sps->frame_crop_top_offset * 2) - (m_h264->sps->frame_crop_bottom_offset * 2);
						format = V4L2_PIX_FMT_H264;
					}
				}
			}
			else if (m_codec == "H265" && nal_size >= 2)
			{
				const int nal_type = (buffer[nal_start] >> 1) & 0x3f;
				if (nal_type == HEVC_NAL_UNIT_TYPE_SPS_NUT &&
					read_hevc_nal_unit(m_hevc, &buffer[nal_start], nal_size) > 0)
				{
					const hevc_sps_t* sps = m_hevc->sps;
					int sub_width_c = 1;
					int sub_height_c = 1;
					if (sps->chroma_format_idc == 1)
					{
						sub_width_c = 2;
						sub_height_c = 2;
					}
					else if (sps->chroma_format_idc == 2)
					{
						sub_width_c = 2;
					}
					if (sps->chroma_format_idc <= 3)
					{
						if (sps->separate_colour_plane_flag)
						{
							sub_width_c = 1;
							sub_height_c = 1;
						}
						const int cropped_width = sps->pic_width_in_luma_samples -
							sub_width_c * (sps->conf_win_left_offset + sps->conf_win_right_offset);
						const int cropped_height = sps->pic_height_in_luma_samples -
							sub_height_c * (sps->conf_win_top_offset + sps->conf_win_bottom_offset);
						if (cropped_width > 0 && cropped_height > 0)
						{
							width = cropped_width;
							height = cropped_height;
							format = V4L2_PIX_FMT_HEVC;
						}
					}
				}
			}
		}

		if (format && width && height)
		{
			LOG(NOTICE) << "geometry:" << width << "x" << height;
			V4L2DeviceParameters outparam(m_out_devname.c_str(), format, width, height, 0, m_ioTypeOut);
			m_videoOutput = V4l2Output::create(outparam);
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
