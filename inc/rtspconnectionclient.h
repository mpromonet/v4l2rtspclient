/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** rtspconnectionclient.h
** 
** Interface to an RTSP client connection
** 
** -------------------------------------------------------------------------*/

#pragma once

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "logger.h"

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"


#define RTSP_CALLBACK(uri, resultCode, resultString) \
static void continueAfter ## uri(RTSPClient* rtspClient, int resultCode, char* resultString) { static_cast<RTSPConnection*>(rtspClient)->continueAfter ## uri(resultCode, resultString); } \
void continueAfter ## uri (int resultCode, char* resultString) \
/**/
u_int8_t marker[] = { 0, 0, 0, 1 }; 

/* ---------------------------------------------------------------------------
**  RTSP client connection interface
** -------------------------------------------------------------------------*/
class RTSPConnection : public RTSPClient
{
	public:
		/* ---------------------------------------------------------------------------
		**  RTSP client callback interface
		** -------------------------------------------------------------------------*/
		class Callback
		{
			public:
				virtual bool onNewSession(const char* media, const char* codec) = 0;
				virtual bool onData(unsigned char* buffer, ssize_t size) = 0;
		};

	protected:
		/* ---------------------------------------------------------------------------
		**  RTSP client Sink
		** -------------------------------------------------------------------------*/
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
						if (!m_callback->onData(m_buffer, frameSize+sizeof(marker)))
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
				else if (m_callback->onNewSession(m_subSession->mediumName(), m_subSession->codecName()))
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

		void start()
		{
			this->sendNextCommand(); 
		}
		
		void mainloop()
		{
			m_env->taskScheduler().doEventLoop(&m_stop);
		}
				
		void stop() { m_stop = 1; };
		
	protected:
		UsageEnvironment*        m_env;
		MediaSession*            m_session;                   
		MediaSubsession*         m_subSession;             
		MediaSubsessionIterator* m_subSessionIter;
		Callback*                m_callback; 	
		char                     m_stop;
};
