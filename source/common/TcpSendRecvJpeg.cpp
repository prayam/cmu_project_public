//------------------------------------------------------------------------------------------------
// File: TcpSendRecvJpeg.cpp
// Project: LG Exec Ed Program
// Versions:
// 1.0 April 2017 - initial version
// Send and receives OpenCV Mat Images in a Tcp Stream commpressed as Jpeg images
//------------------------------------------------------------------------------------------------
#include <opencv2/highgui/highgui.hpp>
#include "TcpSendRecvJpeg.h"
static  int init_values[2] = { cv::IMWRITE_JPEG_QUALITY,80 }; //default(95) 0-100
static  std::vector<int> param (&init_values[0], &init_values[0]+2);
static  std::vector<uchar> sendbuff;//buffer for coding

//-----------------------------------------------------------------
// TcpSendImageAsJpeg - Sends a Open CV Mat Image commressed as a
// jpeg image in side a TCP Stream on the specified TCP local port
// and Destination. return bytes sent on success and -1 on failure
//-----------------------------------------------------------------
int TcpSendImageAsJpeg(TTcpConnectedPort * TcpConnectedPort,cv::Mat Image)
{
	unsigned int imagesize;
	cv::imencode(".jpg", Image, sendbuff, param);
	imagesize=htonl(sendbuff.size()); // convert image size to network format
	if (WriteDataTcp(TcpConnectedPort,(unsigned char *)&imagesize,sizeof(imagesize))!=sizeof(imagesize))
		return(-1);
	return(WriteDataTcp(TcpConnectedPort,sendbuff.data(), sendbuff.size()));
}

//-----------------------------------------------------------------
// END TcpSendImageAsJpeg
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// TcpRecvImageAsJpeg - Sends a Open CV Mat Image commressed as a
// jpeg image in side a TCP Stream on the specified TCP local port
// returns true on success and false on failure
//-----------------------------------------------------------------
int TcpRecvImageAsJpeg(TTcpConnectedPort * TcpConnectedPort,cv::Mat *Image)
{
	ssize_t ret;
	unsigned int imagesize;
	unsigned char *buff;	/* receive buffer */

	ret = ReadDataTcp(TcpConnectedPort, (unsigned char *)&imagesize, sizeof(imagesize));
	if (ret != sizeof(imagesize)) {
		goto exit;
	}

	imagesize = ntohl(imagesize); // convert image size to host format
	buff = new (std::nothrow) unsigned char [imagesize];
	if (buff == NULL) {
		ret = -1;
		g_print("buffer allocation fail\n");
		goto exit;
	}

	ret = ReadDataTcp(TcpConnectedPort, buff, imagesize);
	if(ret == imagesize) {
		cv::imdecode(cv::Mat(imagesize, 1, CV_8UC1, buff), cv::IMREAD_COLOR, Image);
		delete [] buff;

		if (!(*Image).empty()) {
			goto exit;
		}
		else {
			ret = -1;
			g_print("unknown image\n");
			goto exit;
		}
	}
	delete [] buff;
exit:
	return (int)ret;
}

//-----------------------------------------------------------------
// END TcpRecvImageAsJpeg
//-----------------------------------------------------------------
//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------
