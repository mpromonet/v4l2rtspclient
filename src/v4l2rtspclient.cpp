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
#include <signal.h>

#include <fstream>

#include "logger.h"

#include "environment.h"
#include "rtspconnectionclient.h"
#include "v4l2writer.h"


/* ---------------------------------------------------------------------------
**  main
** -------------------------------------------------------------------------*/
int main(int argc, char* argv[]) 
{	
	int verbose=0;
	std::string url;
	const char *out_devname = "/dev/video1";	
	int c = 0;
	V4l2IoType ioTypeOut = V4l2IoType::IOTYPE_MMAP;
	
	while ((c = getopt (argc, argv, "hv::w")) != -1)
	{
		switch (c)
		{
			case 'v':	verbose   = 1; if (optarg && *optarg=='v') verbose++;  break;
			case 'w':	ioTypeOut = V4l2IoType::IOTYPE_READWRITE; break;			
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
	
	
	V4l2Writer callback(out_devname, ioTypeOut);
	Environment env;
	RTSPConnection connnection(env, &callback, url.c_str(), verbose);
	env.mainloop();
	
	return 0;
}
