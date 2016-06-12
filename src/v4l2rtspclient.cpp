/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** v4l2rtspclient.cpp
** 
** Copy RTSP stream to an V4L2 output device
** 
** -------------------------------------------------------------------------*/

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <signal.h>

#include <fstream>

#include "logger.h"

#include "V4l2Device.h"
#include "V4l2Output.h"



/* ---------------------------------------------------------------------------
**  RTSP client wrapper callback
** -------------------------------------------------------------------------*/
class Callback
{
	public:
		virtual bool notifySession(const char* media, const char* codec) = 0;
		virtual bool notifyData(unsigned char* buffer, ssize_t size) = 0;
};


/* ---------------------------------------------------------------------------
**  RTSP client wrapper
** -------------------------------------------------------------------------*/
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"

#define RTSP_CALLBACK(uri, resultCode, resultString) \
static void continueAfter ## uri(RTSPClient* rtspClient, int resultCode, char* resultString) { static_cast<RTSPConnection*>(rtspClient)->continueAfter ## uri(resultCode, resultString); } \
void continueAfter ## uri (int resultCode, char* resultString) \
/**/
u_int8_t marker[] = { 0, 0, 0, 1 }; 
class RTSPConnection : public RTSPClient
{
	class SessionSink: public MediaSink 
	{
		public:
			static SessionSink* createNew(UsageEnvironment& env, Callback* callback) { return new SessionSink(env, callback); }

		private:
			SessionSink(UsageEnvironment& env, Callback* callback) : MediaSink(env), m_bufferSize(1024*1024), m_callback(callback) 
			{
				m_buffer = new u_int8_t[m_bufferSize];
				memcpy(m_buffer, marker, sizeof(marker));
			}
			
			virtual ~SessionSink()
			{
				delete [] m_buffer;
			}

			static void afterGettingFrame(void* clientData, unsigned frameSize,
						unsigned numTruncatedBytes,
						struct timeval presentationTime,
						unsigned durationInMicroseconds)
			{
				static_cast<SessionSink*>(clientData)->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
			}
			
			void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes, struct timeval presentationTime, unsigned durationInMicroseconds)
			{
				this->envir() << "NOTIFY size:" << frameSize << "\n";
				if (numTruncatedBytes != 0)
				{
					delete [] m_buffer;
					m_bufferSize *= 2;
					this->envir() << "buffer too small, reallocate bigger one\n";
					m_buffer = new u_int8_t[m_bufferSize];
					memcpy(m_buffer, marker, sizeof(marker));
				}
				else
				{
					if (!m_callback->notifyData(m_buffer, frameSize+sizeof(marker)))
					{
						this->envir() << "NOTIFY failed\n";
					}
				}
				this->continuePlaying();
			}

		private:
			virtual Boolean continuePlaying()
			{
				Boolean ret = False;
				if (source() != NULL)
				{
					source()->getNextFrame(m_buffer+sizeof(marker), m_bufferSize-sizeof(marker),
							afterGettingFrame, this,
							onSourceClosure, this);
					ret = True;
				}
				return ret;	
			}

		private:
			size_t    m_bufferSize;
			u_int8_t* m_buffer;
			Callback* m_callback; 	
	};
	
	public:
		RTSPConnection(Callback* callback, const std::string & rtspURL, int verbosityLevel = 255) 
						: RTSPClient(*BasicUsageEnvironment::createNew(*BasicTaskScheduler::createNew()), rtspURL.c_str(), verbosityLevel, NULL, 0
#if LIVEMEDIA_LIBRARY_VERSION_INT > 1371168000 
							,-1
#endif
							)
						, m_env(&this->envir())
						, m_session(NULL)
						, m_subSessionIter(NULL)
						, m_callback(callback)
						, m_stop(0)
		{
		}
		
		virtual ~RTSPConnection()
		{
			delete m_subSessionIter;
			Medium::close(m_session);
			TaskScheduler* scheduler = &m_env->taskScheduler();
			m_env->reclaim();
			delete scheduler;
		}
				
		void sendNextCommand() 
		{
			if (m_subSessionIter == NULL)
			{
				this->sendDescribeCommand(continueAfterDESCRIBE); 
			}
			else
			{
				m_subSession = m_subSessionIter->next();
				if (m_subSession != NULL) 
				{
					if (!m_subSession->initiate()) 
					{
						*m_env << "Failed to initiate " << m_subSession->mediumName() << "/" << m_subSession->codecName() << " subsession: " << m_env->getResultMsg() << "\n";
						this->sendNextCommand();
					} 
					else 
					{					
						*m_env << "Initiated " << m_subSession->mediumName() << "/" << m_subSession->codecName() << " subsession\n";
					}

					this->sendSetupCommand(*m_subSession, continueAfterSETUP);
				}
				else
				{
					this->sendPlayCommand(*m_session, continueAfterPLAY);
				}
			}
		}
				
		RTSP_CALLBACK(DESCRIBE,resultCode,resultString)
		{
			if (resultCode != 0) 
			{
				*m_env << "Failed to get a SDP description: " << resultString << "\n";
			}
			else
			{
				char* const sdpDescription = resultString;
				*m_env << "Got a SDP description:\n" << sdpDescription << "\n";
				m_session = MediaSession::createNew(*m_env, sdpDescription);
				m_subSessionIter = new MediaSubsessionIterator(*m_session);
				this->sendNextCommand();  
			}
			delete[] resultString;
		}
		
		RTSP_CALLBACK(SETUP,resultCode,resultString)
		{
			if (resultCode != 0) 
			{
				*m_env << "Failed to SETUP: " << resultString << "\n";
			}
			else
			{				
				m_subSession->sink = SessionSink::createNew(*m_env, m_callback);
				if (m_subSession->sink == NULL) 
				{
					*m_env << "Failed to create a data sink for " << m_subSession->mediumName() << "/" << m_subSession->codecName() << " subsession: " << m_env->getResultMsg() << "\n";
				}
				else if (m_callback->notifySession(m_subSession->mediumName(), m_subSession->codecName()))
				{
					*m_env << "Created a data sink for the \"" << m_subSession << "\" subsession\n";
					m_subSession->sink->startPlaying(*(m_subSession->readSource()), NULL, NULL);
				}
			}
			delete[] resultString;
			this->sendNextCommand();  
		}	
		
		RTSP_CALLBACK(PLAY,resultCode,resultString)
		{
			if (resultCode != 0) 
			{
				*m_env << "Failed to PLAY: " << resultString << "\n";
			}
			else
			{
				*m_env << "PLAY OK " << "\n";
			}
			delete[] resultString;
		}
		
		void mainloop()
		{
			this->sendNextCommand(); 
			m_env->taskScheduler().doEventLoop(&m_stop);
		}
				
		void stop() { m_stop = 1; };
		
	protected:
		UsageEnvironment* m_env;
		MediaSession* m_session;                   
		MediaSubsession* m_subSession;             
		MediaSubsessionIterator* m_subSessionIter;
		Callback* m_callback; 	
		char m_stop;
};


#include "h264_stream.h"
class V4l2OutputCallback : public Callback
{
	public:
		V4l2OutputCallback(const std::string &out_devname, V4l2DeviceFactory::IoType ioTypeOut) : m_videoOutput(NULL), m_out_devname(out_devname), m_ioTypeOut(ioTypeOut) 
		{
			m_h264 = h264_new();
		}
		virtual ~V4l2OutputCallback()
		{
			delete m_videoOutput;
			h264_free(m_h264);
		}
	
		virtual bool notifySession(const char* media, const char* codec)
		{
			bool success = false;
			if ( (strcmp(media, "video") == 0) && (strcmp(codec, "H264") == 0) )
			{
				success = true;
			}
			return success;
		}
		virtual bool notifyData(unsigned char* buffer, ssize_t size)
		{
			bool success = false;

			if (m_videoOutput == NULL)
			{			
				int nal_start = 0;
				int nal_end   = 0;
				find_nal_unit(buffer, size, &nal_start, &nal_end);
				read_nal_unit(m_h264, &buffer[nal_start], nal_end - nal_start);
				if (m_h264->nal->nal_unit_type == NAL_UNIT_TYPE_SPS)
				{
					unsigned int width = ((m_h264->sps->pic_width_in_mbs_minus1 +1)*16) - m_h264->sps->frame_crop_left_offset*2 - m_h264->sps->frame_crop_right_offset*2;
					unsigned int height= ((2 - m_h264->sps->frame_mbs_only_flag)* (m_h264->sps->pic_height_in_map_units_minus1 +1) * 16) - (m_h264->sps->frame_crop_top_offset * 2) - (m_h264->sps->frame_crop_bottom_offset * 2);
					std::cout << "geometry:" << width << "x" << height << "\n";
					
					V4L2DeviceParameters outparam(m_out_devname.c_str(), V4L2_PIX_FMT_H264, width, height, 0, 255);
					m_videoOutput = V4l2DeviceFactory::CreateVideoOutput(outparam, m_ioTypeOut);			
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
			
	private:
		V4l2Output*               m_videoOutput;
		std::string               m_out_devname;
		V4l2DeviceFactory::IoType m_ioTypeOut;
		h264_stream_t*            m_h264; 
};

/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{	
	int verbose=0;
	std::string url;
	const char *out_devname = "/dev/video1";	
	int c = 0;
	V4l2DeviceFactory::IoType ioTypeOut = V4l2DeviceFactory::IOTYPE_MMAP;
	
	while ((c = getopt (argc, argv, "hv::w")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose   = 1; if (optarg && *optarg=='v') verbose++;  break;
			case 'w':	ioTypeOut = V4l2DeviceFactory::IOTYPE_READ; break;			
			case 'h':
			{
				std::cout << argv[0] << " [-v[v]] rtsp_url dest_device" << std::endl;
				std::cout << "\t -v            : verbose " << std::endl;
				std::cout << "\t -vv           : very verbose " << std::endl;
				std::cout << "\t -w            : V4L2 capture using write interface (default use memory mapped buffers)" << std::endl;
				std::cout << "\t rtsp_url      : RTSP url to connect" << std::endl;
				std::cout << "\t dest_device   : V4L2 capture device (default "<< out_devname << ")" << std::endl;
				exit(0);
			}
		}
	}
	if (optind<argc)
	{
		url = argv[optind];
		optind++;
	}	
	if (optind<argc)
	{
		out_devname = argv[optind];
		optind++;
	}	

	// initialize log4cpp
	initLogger(verbose);
	
	
	V4l2OutputCallback callback(out_devname, ioTypeOut);
	RTSPConnection connnection(&callback, url);
	connnection.mainloop();
	
	return 0;
}
