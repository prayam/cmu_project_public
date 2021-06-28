
#include <iostream>
#include <string>
#include <chrono>

#include <opencv2/core/cuda.hpp>
#include <opencv2/cudawarping.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <termios.h>

#include <sys/stat.h>
#include <sys/types.h>

#define err_ssl()       do { } while(0)
#define err_ssle(e)     do { } while(0)
#define err_file()      do { } while(0)
#define err_mem()       do { } while(0)
#define err_param()     do { } while(0)
#define err_cmm()       do { } while(0)
#define err_msg(M, ...) do { } while(0)
#define log_d(...)      do { } while(0)
#define ERR_SSL_KEY         100
#define ERR_SSL_INIT        101
#define ERR_SSL_UPDATE      102
#define ERR_SSL_FINAL       103
#define ERR_SSL_SIGFORMAT   104
#define ERR_FILE            200
#define ERR_FILE_OPEN       201
#define ERR_FILE_READ       202
#define ERR_FILE_WRITE      203
#define ERR_FILE_CLOSE      204
#define ERR_FILE_EXIST      205
#define ERR_FILE_CURRUPT    206
#define ERR_MALLOC_NULL     300
#define ERR_COMMON          400
#define ERR_PARAMETER       500
#define ERR_KEY_GEN         600
#define ERR_KEY_CURRUPT     601
#define ERR_DATA_CURRUPT    700

#define MAXFACES	8

int load_file(const char* fname, unsigned char** data, size_t* data_len)
{

    int err = 0;
    uint32_t rwlen = 0;

    if ((fname == NULL) || (data == NULL) || (data_len == NULL)) {
        err_param();
        return ERR_PARAMETER;
    }
    FILE* fp = fopen(fname, "rb");
    if (fp == NULL) {
        err_file();
        err = ERR_FILE_OPEN;
        goto _error;
    }

    struct stat finfo;
    if (fstat(fileno(fp), &finfo) == -1) {
        err_file();
        err = ERR_FILE_OPEN;
        goto _error;
    }
    *data_len = finfo.st_size;
    rewind(fp);

    *data = (unsigned char*) malloc(*data_len+1);
    if (*data == NULL) {
        err_mem();
        err = ERR_MALLOC_NULL;
        goto _error;
    } else {
        memset(*data, 0, *data_len+1);
    }

    rwlen = fread(*data, sizeof(char), *data_len, fp);
    if ((rwlen != (uint32_t)(*data_len)) && (!feof(fp))) {
        err_file();
        err = ERR_FILE_READ;
        goto _error;
    } else rwlen = 0;


    _error:  // Free Resources
    if ((fp != NULL) && (fclose(fp) != 0)) err_file();

    return err;
}

int save_file(const char* fname, unsigned char* data, size_t data_len)
{

    if ((data_len < 0) || (data_len >= ULONG_MAX)) return ERR_PARAMETER;

    int err = 0;
    uint32_t rwlen = 0;

    if ((fname == NULL) || (data == NULL)) {
        err_param();
        return ERR_PARAMETER;
    }
    FILE* fp = fopen(fname, "wb");
    if (fp == NULL) {
        err_file();
        err = ERR_FILE_OPEN;
        goto _error;
    }
    rwlen = fwrite(data, sizeof(char), data_len, fp);
    if (rwlen != (uint32_t) data_len) {
        err_file();
        err = ERR_FILE_WRITE;
        goto _error;
    } else rwlen = 0;

    _error:  // Free Resources
    if ((fp != NULL) && (fclose(fp) != 0)) err_file();

    return err;
}

int main(int argc, char *argv[])
{
    int videoFrameWidth = 640;
    int videoFrameHeight = 480;

    int maxFacesPerScene = 8;

	unsigned char *buf;
	size_t size;
    int err = 0;

    char* filename = argv[1];
    err = load_file(filename, &buf, &size);

	cv::Mat image = cv::Mat(videoFrameHeight, videoFrameWidth, 16);
    image.data = buf;

	std::vector<uchar> pic_buf;
	cv::imencode(".jpg", image, pic_buf);

    err = save_file("./result.jpg", pic_buf.data(), pic_buf.size());

    if (buf != NULL) { free(buf); buf = NULL; }
	return 0;
}




