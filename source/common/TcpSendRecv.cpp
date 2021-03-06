//------------------------------------------------------------------------------------------------
// File: TcpSendRecvJpeg.cpp
// Project: LG Exec Ed Program
// Versions:
// 1.0 April 2017 - initial version
// Send and receives OpenCV Mat Images in a Tcp Stream commpressed as Jpeg images
//------------------------------------------------------------------------------------------------
#include <glib.h>
#include <opencv2/highgui/highgui.hpp>
#include "TcpSendRecv.h"
#include "CommonStruct.h"
#include "Logger.h"

static  gint init_values[2] = { cv::IMWRITE_JPEG_QUALITY,80 }; //default(95) 0-100
static  std::vector<gint> param (&init_values[0], &init_values[0]+2);
static  std::vector<uchar> sendbuff;//buffer for coding

//-----------------------------------------------------------------
// TcpSendImageAsJpeg - Sends a Open CV Mat Image commressed as a
// jpeg image in side a TCP Stream on the specified TCP local port
// and Destination. return bytes sent on success and -1 on failure
//-----------------------------------------------------------------
gint TcpSendImageAsJpeg(TTcpConnectedPort * TcpConnectedPort,cv::Mat Image)
{
	guint imagesize;
	cv::imencode(".jpg", Image, sendbuff, param);
	imagesize=htonl(sendbuff.size()); // convert image size to network format
	if (WriteDataTcp(TcpConnectedPort,(guchar *)&imagesize,sizeof(imagesize))!=sizeof(imagesize))
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
gssize TcpRecvImageAsJpeg(TTcpConnectedPort * TcpConnectedPort,cv::Mat *Image)
{
	gssize ret;
	guint imagesize;
	guchar *buff = NULL;	/* receive buffer */

	ret = ReadDataTcp(TcpConnectedPort, (guchar *)&imagesize, sizeof(imagesize));
	if (ret != sizeof(imagesize)) {
		LOG_WARNING("fail ReadDataTcp");
		goto exit;
	}

	imagesize = ntohl(imagesize); // convert image size to host format
	buff = (guchar*)g_malloc(imagesize);
	if (buff == NULL) {
		ret = -1;
		LOG_WARNING("buffer allocation fail");
		goto exit;
	}

	ret = ReadDataTcp(TcpConnectedPort, buff, imagesize);
	if(ret == imagesize) {
		cv::imdecode(cv::Mat(imagesize, 1, CV_8UC1, buff), cv::IMREAD_COLOR, Image);

		if ((*Image).empty()) {
			ret = -1;
			LOG_WARNING("unknown image");
			goto exit;
		}
	}

exit:
	if (buff)
		g_free(buff);
	return ret;
}
//-----------------------------------------------------------------
// END TcpRecvImageAsJpeg
//-----------------------------------------------------------------


gboolean TcpSendLoginData(TTcpConnectedPort * TcpConnectedPort, const gchar* userid, const gchar* userpw)
{
	gboolean ret = FALSE;
	struct APP_command_req *req = NULL;
	guint8 *tmp;
	gsize userid_len, userpw_len, req_len, write_ret;

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	if (userid == NULL || userpw == NULL) {
		LOG_WARNING("userid or userpw is NULL");
		goto exit;
	}

	userid_len = strlen(userid);
	userpw_len = strlen(userpw);
	if (userid_len > MAX_ACCOUNT_ID || userpw_len > MAX_ACCOUNT_PW) {
		LOG_WARNING("expected userid max len(%d), but it's (%ld)", MAX_ACCOUNT_ID, userid_len);
		LOG_WARNING("expected userpw max len(%d), but it's (%ld)", MAX_ACCOUNT_PW, userpw_len);
		goto exit;
	}

	if (MAX_DATA_LEN < (1 + userid_len + 1 + userpw_len)) {
		LOG_WARNING("expected data max len(%d), but it's (%ld)", MAX_DATA_LEN, (1 + userid_len + 1 + userpw_len));
		goto exit;
	}

	req_len = sizeof(struct APP_command_req);
	req = g_new0(struct APP_command_req, 1);
	if (req == NULL) {
		LOG_WARNING("memory allocation fail");
		goto exit;
	}

	req->req_id = REQ_LOGIN;
	tmp = (guint8*)req->data;

	*tmp = (guint8)userid_len;
	tmp++;
	memcpy(tmp, userid, userid_len);
	tmp += userid_len;

	*tmp = (guint8)userpw_len;
	tmp++;
	memcpy(tmp, userpw, userpw_len);
	tmp += userpw_len;

	write_ret = WriteDataTcp(TcpConnectedPort, (guchar *)req, req_len);
	if (write_ret != req_len) {
		LOG_WARNING("unexpected write len %ld. it should be %ld", write_ret, req_len);
		goto exit;
	}

	ret = TRUE;
	LOG_DEBUG("done TcpSendLoginData");

exit:
	if (req)
		g_free(req);
	return ret;
}

/* CAUTION: Caller should free userid and userpw after using them. */
gssize TcpRecvLoginData(TTcpConnectedPort * TcpConnectedPort, gchar** userid, gchar** userpw)
{
	gssize read_len = TCP_RECV_ERROR;
	gssize req_len;
	struct APP_command_req *req = NULL;
	guint8 *tmp;
	guint8 userid_len, userpw_len;

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	req = g_new0(struct APP_command_req, 1);
	if (req == NULL) {
		LOG_WARNING("memory allocation fail");
		goto exit;
	}

	req_len = sizeof(struct APP_command_req);
	read_len = ReadDataTcp(TcpConnectedPort, (guchar *)req, req_len);
	if (read_len != req_len) {
		LOG_WARNING("unexpected read len %ld. it should be %ld", read_len, req_len);
		goto exit;
	}

	if (req->req_id != REQ_LOGIN) {
		LOG_WARNING("unexpected message id %d. it should be %d", req->req_id, REQ_LOGIN);
		goto exit;
	}

	tmp = (guint8*)req->data;
	userid_len = *tmp;
	tmp++;
	if (userid_len > MAX_ACCOUNT_ID) {
		LOG_WARNING("exceeds id len %d. read len %d", MAX_ACCOUNT_ID, userid_len);
		goto exit;
	}
	tmp += userid_len;

	userpw_len = *tmp;
	if (userpw_len > MAX_ACCOUNT_PW) {
		LOG_WARNING("exceeds pw len %d. read len %d", MAX_ACCOUNT_PW, userpw_len);
		goto exit;
	}

	*userid = g_strndup (req->data + 1, userid_len);
	if (*userid == NULL) {
		LOG_WARNING("memory allocation fail");
		goto exit;
	}

	*userpw = g_strndup (req->data + 1 + userid_len + 1, userpw_len);
	if (*userpw == NULL) {
		LOG_WARNING("memory allocation fail");
		g_free(*userid);
		*userid = NULL;
		goto exit;
	}

	LOG_DEBUG("done TcpRecvLoginData");

exit:
	if (req)
		g_free(req);
	return read_len;
}

gboolean TcpSendLogoutReq(TTcpConnectedPort * TcpConnectedPort)
{
	gboolean ret = FALSE;
	struct APP_command_req *req = NULL;
	gsize req_len, write_ret;

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	req = g_new0(struct APP_command_req, 1);
	if (req == NULL) {
		LOG_WARNING("memory allocation fail");
		goto exit;
	}

	req->req_id = REQ_LOGOUT;

	req_len = sizeof(struct APP_command_req);
	write_ret = WriteDataTcp(TcpConnectedPort, (guchar *)req, req_len);
	if (write_ret != req_len) {
		LOG_WARNING("unexpected write len %ld. it should be %ld", write_ret, req_len);
		goto exit;
	}

	ret = TRUE;
	LOG_DEBUG("done TcpSendLogoutReq");

exit:
	if (req)
		g_free(req);
	return ret;
}

gboolean TcpSendSecureModeReq(TTcpConnectedPort * TcpConnectedPort)
{
	gboolean ret = FALSE;
	struct APP_command_req *req = NULL;
	gsize req_len, write_ret;

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	req = g_new0(struct APP_command_req, 1);
	if (req == NULL) {
		LOG_WARNING("memory allocation fail");
		goto exit;
	}

	req->req_id = REQ_SECURE;

	req_len = sizeof(struct APP_command_req);
	write_ret = WriteDataTcp(TcpConnectedPort, (guchar *)req, req_len);
	if (write_ret != req_len) {
		LOG_WARNING("unexpected write len %ld. it should be %ld", write_ret, req_len);
		goto exit;
	}

	ret = TRUE;
	LOG_DEBUG("done TcpSendSecureModeReq");

exit:
	if (req)
		g_free(req);
	return ret;
}

gboolean TcpSendNonSecureModeReq(TTcpConnectedPort * TcpConnectedPort)
{
	gboolean ret = FALSE;
	struct APP_command_req *req = NULL;
	gsize req_len, write_ret;

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	req = g_new0(struct APP_command_req, 1);
	if (req == NULL) {
		LOG_WARNING("memory allocation fail");
		goto exit;
	}

	req->req_id = REQ_NONSECURE;

	req_len = sizeof(struct APP_command_req);
	write_ret = WriteDataTcp(TcpConnectedPort, (guchar *)req, req_len);
	if (write_ret != req_len) {
		LOG_WARNING("unexpected write len %ld. it should be %ld", write_ret, req_len);
		goto exit;
	}

	ret = TRUE;
	LOG_DEBUG("done TcpSendNonSecureModeReq");

exit:
	if (req)
		g_free(req);
	return ret;
}

gboolean TcpSendTestRunModeReq(TTcpConnectedPort * TcpConnectedPort)
{
	gboolean ret = FALSE;
	struct APP_command_req *req = NULL;
	gsize req_len, write_ret;

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	req = g_new0(struct APP_command_req, 1);
	if (req == NULL) {
		LOG_WARNING("memory allocation fail");
		goto exit;
	}

	req->req_id = REQ_TESTRUN;

	req_len = sizeof(struct APP_command_req);
	write_ret = WriteDataTcp(TcpConnectedPort, (guchar *)req, req_len);
	if (write_ret != req_len) {
		LOG_WARNING("unexpected write len %ld. it should be %ld", write_ret, req_len);
		goto exit;
	}

	ret = TRUE;
	LOG_DEBUG("done TcpSendTestRunModeReq");

exit:
	if (req)
		g_free(req);
	return ret;
}

gboolean TcpSendRunModeReq(TTcpConnectedPort * TcpConnectedPort)
{
	gboolean ret = FALSE;
	struct APP_command_req *req = NULL;
	gsize req_len, write_ret;

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	req = g_new0(struct APP_command_req, 1);
	if (req == NULL) {
		LOG_WARNING("memory allocation fail");
		goto exit;
	}

	req->req_id = REQ_RUN;

	req_len = sizeof(struct APP_command_req);
	write_ret = WriteDataTcp(TcpConnectedPort, (guchar *)req, req_len);
	if (write_ret != req_len) {
		LOG_WARNING("unexpected write len %ld. it should be %ld", write_ret, req_len);
		goto exit;
	}

	ret = TRUE;
	LOG_DEBUG("done TcpSendRunModeReq");

exit:
	if (req)
		g_free(req);
	return ret;
}

gboolean TcpSendCaptureReq(TTcpConnectedPort * TcpConnectedPort)
{
	gboolean ret = FALSE;
	struct APP_command_req *req = NULL;
	gsize req_len, write_ret;

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	req = g_new0(struct APP_command_req, 1);
	if (req == NULL) {
		LOG_WARNING("memory allocation fail");
		goto exit;
	}

	req->req_id = REQ_CAPTURE;

	req_len = sizeof(struct APP_command_req);
	write_ret = WriteDataTcp(TcpConnectedPort, (guchar *)req, req_len);
	if (write_ret != req_len) {
		LOG_WARNING("unexpected write len %ld. it should be %ld", write_ret, req_len);
		goto exit;
	}

	ret = TRUE;
	LOG_DEBUG("done TcpSendCaptureReq");

exit:
	if (req)
		g_free(req);
	return ret;
}

gboolean TcpSendSaveReq(TTcpConnectedPort * TcpConnectedPort, const gchar *name)
{
	gboolean ret = FALSE;
	struct APP_command_req *req = NULL;
	guint8 *tmp;
	gsize name_len, req_len, write_ret;

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	if (name == NULL) {
		LOG_WARNING("name is NULL");
		goto exit;
	}

	name_len = strlen(name);
	if (name_len > MAX_NAME) {
		LOG_WARNING("expected userpw max len(%d), but it's (%ld)", MAX_NAME, name_len);
		goto exit;
	}

	if (MAX_DATA_LEN < (1 + name_len)) {
		LOG_WARNING("expected data max len(%d), but it's (%ld)", MAX_DATA_LEN, (1 + name_len));
		goto exit;
	}

	req_len = sizeof(struct APP_command_req);
	req = g_new0(struct APP_command_req, 1);
	if (req == NULL) {
		LOG_WARNING("memory allocation fail");
		goto exit;
	}

	req->req_id = REQ_SAVE;
	tmp = (guint8*)req->data;

	*tmp = (guint8)name_len;
	tmp++;
	memcpy(tmp, name, name_len);

	write_ret = WriteDataTcp(TcpConnectedPort, (guchar *)req, req_len);
	if (write_ret != req_len) {
		LOG_WARNING("unexpected write len %ld. it should be %ld", write_ret, req_len);
		goto exit;
	}

	ret = TRUE;
	LOG_DEBUG("done TcpSendSaveReq");

exit:
	if (req)
		g_free(req);
	return ret;
}

/* CAUTION: Caller should free data after using it. */
gssize TcpRecvCtrlReq(TTcpConnectedPort * TcpConnectedPort, gchar *req_id, void **data)
{
	gssize read_len = TCP_RECV_ERROR;
	gssize req_len;
	struct APP_command_req *req = NULL;
	guint8 *tmp;
	guint8 name_len;

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	req = g_new0(struct APP_command_req, 1);
	if (req == NULL) {
		LOG_WARNING("memory allocation fail");
		goto exit;
	}

	req_len = sizeof(struct APP_command_req);
	read_len = ReadDataTcp(TcpConnectedPort, (guchar *)req, req_len);
	if (read_len != req_len) {
		LOG_WARNING("unexpected read len %ld. it should be %ld", read_len, req_len);
		goto exit;
	}

	*req_id = req->req_id;
	if (req->req_id == REQ_SAVE) {
		tmp = (guint8*)req->data;
		name_len = *tmp;
		tmp++;
		if (name_len > MAX_NAME) {
			LOG_WARNING("exceeds id len %d. read len %d", MAX_NAME, name_len);
			goto exit;
		}

		*data = g_strndup (req->data + 1, name_len);
		if (*data == NULL) {
			LOG_WARNING("memory allocation fail");
			goto exit;
		}
	}

	LOG_DEBUG("done TcpRecvCtrlReq");

exit:
	if (req)
		g_free(req);
	return read_len;
}

gint TcpSendRes(TTcpConnectedPort * TcpConnectedPort, guchar res)
{
	return WriteDataTcp(TcpConnectedPort, &res, sizeof(gchar));
}

gssize TcpRecvRes(TTcpConnectedPort * TcpConnectedPort, guchar *res)
{
	return ReadDataTcp(TcpConnectedPort, res, sizeof(gchar));
}

gboolean TcpSendMeta(TTcpConnectedPort * TcpConnectedPort, std::vector<struct APP_meta> meta)
{
	gsize write_ret;
	guint8 nr = meta.size();

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	write_ret = WriteDataTcp(TcpConnectedPort, (guchar *)&nr, sizeof(guint8));
	if (write_ret != sizeof(guint8)) {
		LOG_WARNING("Meta/nr: unexpected write len %ld. it should be %ld", write_ret, sizeof(guint8));
		goto exit;
	}

	for (gint i = 0; i < nr; i++) {
		struct APP_meta tmp;

		g_strlcpy(tmp.name, meta[i].name, MAX_NAME - 1);
		tmp.x1 = htonl(meta[i].x1);
		tmp.y1 = htonl(meta[i].y1);
		tmp.x2 = htonl(meta[i].x2);
		tmp.y2 = htonl(meta[i].y2);

		write_ret = WriteDataTcp(TcpConnectedPort, (guchar *)&tmp, sizeof(struct APP_meta));
		if (write_ret != sizeof(struct APP_meta)) {
			LOG_WARNING("Meta/item: unexpected write len %ld. it should be %ld", write_ret, sizeof(struct APP_meta));
			goto exit;
		}
	}
	return TRUE;
exit:
	return FALSE;
}

gssize TcpRecvMeta(TTcpConnectedPort * TcpConnectedPort, std::vector<struct APP_meta> &meta)
{
	gsize read_ret = TCP_RECV_ERROR;
	guint8 nr;

	if (TcpConnectedPort == NULL) {
		LOG_WARNING("TcpConnectedPort is NULL");
		goto exit;
	}

	read_ret = ReadDataTcp(TcpConnectedPort, (guchar *)&nr, sizeof(guint8));
	if (read_ret != sizeof(guint8)) {
		LOG_WARNING("Meta/nr: unexpected read len %ld. it should be %ld", read_ret, sizeof(guint8));
		goto exit;
	}

	for (gint i = 0; i < nr; i++) {
		struct APP_meta tmp;

		read_ret = ReadDataTcp(TcpConnectedPort, (guchar *)&tmp, sizeof(struct APP_meta));
		if (read_ret != sizeof(struct APP_meta)) {
			LOG_WARNING("Meta/item: unexpected read len %ld. it should be %ld", read_ret, sizeof(struct APP_meta));
			goto exit;
		}

		tmp.x1 = ntohl(tmp.x1);
		tmp.y1 = ntohl(tmp.y1);
		tmp.x2 = ntohl(tmp.x2);
		tmp.y2 = ntohl(tmp.y2);

		meta.push_back(tmp);
	}

exit:
	return read_ret;
}

//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------
