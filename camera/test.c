#include "opencv2/highgui/highgui.hpp"
#include <opencv/highgui.h>
#include <opencv2/imgproc/imgproc.hpp>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <assert.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include "unistd.h"

#define VIDEO_NUMBER "/dev/video1"
//#define VIDEO_FORMAT V4L2_PIX_FMT_RGB32
#define VIDEO_FORMAT V4L2_PIX_FMT_MJPEG
//#define VIDEO_FORMAT V4L2_PIX_FMT_NV21
//#define VIDEO_FORMAT V4L2_PIX_FMT_JPEG
                   //V4L2_PIX_FMT_JPEG
                   //V4L2_PIX_FMT_YUYV
                   //V4L2_PIX_FMT_YVU420
                   //V4L2_PIX_FMT_RGB32
#define VIDEO_IMAGEWIDTH  640
#define VIDEO_IMAGEHEIGHT 480

using namespace cv;
using namespace std;

typedef struct
{
        void *start;
        int length;
}BUFTYPE;

BUFTYPE user_buf_real;
BUFTYPE *user_buf=&user_buf_real;
int n_buffer = 0;


//open camera device
int open_camer_device()
{
        printf("open_camer_device \n");
        int fd;
        if((fd = open(VIDEO_NUMBER,O_RDWR )) < 0)
        {
                perror("Fail to open");
                exit(EXIT_FAILURE);
        }
        return fd;
}

int init_mmap(int fd)
{
        printf("init_mmap \n");
        int i = 0;
        struct v4l2_requestbuffers reqbuf;

        bzero(&reqbuf,sizeof(reqbuf));
        reqbuf.count = 1;
        reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        reqbuf.memory = V4L2_MEMORY_MMAP;

        //request video buffer, this buffer exist in kernel space, we need use mmap to map it
//this step may modify reqbuf.count value, modify it to the actual buf number.
        if(-1 == ioctl(fd,VIDIOC_REQBUFS,&reqbuf))
        {
                perror("Fail to ioctl 'VIDIOC_REQBUFS'");
                exit(EXIT_FAILURE);
        }

        n_buffer = reqbuf.count; //mark reatch here

        printf("n_buffer = %d\n",n_buffer);

        user_buf->start = calloc(reqbuf.count,sizeof(*user_buf));
        printf("start %d \n",user_buf->start);

        if(user_buf == NULL){
                fprintf(stderr,"Out of memory\n");
                exit(EXIT_FAILURE);
        }

        //Map kernel buffer to user thread space
        for(i = 0; i < reqbuf.count; i ++)
        {
                struct v4l2_buffer buf;

                bzero(&buf,sizeof(buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                printf("index %d \n",i);
               
//Query the info requested from kernel buf
                if(-1 == ioctl(fd,VIDIOC_QUERYBUF,&buf))
                {
                        perror("Fail to ioctl : VIDIOC_QUERYBUF");
                        exit(EXIT_FAILURE);
                }

                user_buf[i].length = buf.length;
                user_buf[i].start =  mmap(
                                        NULL,/*start anywhere*/
                                        buf.length,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED,
                                        fd,buf.m.offset
                                );
                if(MAP_FAILED == user_buf[i].start)
                {
                        perror("Fail to mmap");
                        exit(EXIT_FAILURE);
                }
        }       

        return 0;
}

//Initial camera device
int init_camer_device(int fd)
{
  printf("Initialize camera device \n");

        struct v4l2_fmtdesc fmt;
        struct v4l2_capability cap;
        struct v4l2_format stream_fmt;
        int ret;

        //Video format the current video device support
        memset(&fmt,0,sizeof(fmt));
        fmt.index = 0;
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        while((ret = ioctl(fd,VIDIOC_ENUM_FMT,&fmt)) == 0)
        {
                fmt.index ++ ;

                printf("{pixelformat = %c%c%c%c},description = '%s'\n",
                                fmt.pixelformat & 0xff,(fmt.pixelformat >> 8)&0xff,
                                (fmt.pixelformat >> 16) & 0xff,(fmt.pixelformat >> 24)&0xff,
                                fmt.description);
        }

        //Enqury video device driver's function
        ret = ioctl(fd,VIDIOC_QUERYCAP,&cap);
        if(ret < 0){
                perror("FAIL to ioctl VIDIOC_QUERYCAP");
                exit(EXIT_FAILURE);
        }

        //Judge if it is a video capture device

        if(!(cap.capabilities & V4L2_BUF_TYPE_VIDEO_CAPTURE))
        {
                printf("The Current device is not a video capture device\n");
                exit(EXIT_FAILURE);
        }

        //Judge if ti is support streaming format
        if(!(cap.capabilities & V4L2_CAP_STREAMING))
        //if(!(cap.capabilities & V4L2_BUF_TYPE_VIDEO_CAPTURE))
        {
                printf("The Current device does not support streaming i/o\n");
                exit(EXIT_FAILURE);
        }

        //set video acquiration data format, such as length, width, foramt(JPEG,YUYV,MJPEG等格式)
        stream_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        stream_fmt.fmt.pix.width = VIDEO_IMAGEWIDTH;
        stream_fmt.fmt.pix.height = VIDEO_IMAGEHEIGHT;
        stream_fmt.fmt.pix.pixelformat = VIDEO_FORMAT;
        stream_fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

        if(-1 == ioctl(fd,VIDIOC_S_FMT,&stream_fmt))
        {
                perror("Fail to ioctl");
                exit(EXIT_FAILURE);
        }

        //Initialize video acquire methord(mmap)
        init_mmap(fd);

        return 0;
}

int start_capturing(int fd)
{
        printf("start_capturing \n");
        unsigned int i;
        enum v4l2_buf_type type;

        //n_buffer = 4;
        printf("n_buffer = %d \n",n_buffer);

        //Put the kernel buffer requested previous to a queue
        for(i = 0;i < n_buffer;i ++)
        {
                struct v4l2_buffer buf;

                printf("index %d \n",i);
                bzero(&buf,sizeof(buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if(-1 == ioctl(fd,VIDIOC_QBUF,&buf))
                {
                        perror("Fail to ioctl 'VIDIOC_QBUF'");
                        exit(EXIT_FAILURE);
                }
        }

        //Start acquire data
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(-1 == ioctl(fd,VIDIOC_STREAMON,&type))
        {
                printf("i = %d.\n",i);
                perror("Fail to ioctl 'VIDIOC_STREAMON'");
                exit(EXIT_FAILURE);
        }

        return 0;
}

//Write the acquired data to file
int process_image(void *addr,int length)
{
        FILE *fp;
        static int num = 0;
        char picture_name[20];

        sprintf(picture_name,"Camera_Picture%d.jpg",num ++);

        if((fp = fopen(picture_name,"w")) == NULL)
        {
                perror("Fail to fopen");
                exit(EXIT_FAILURE);
        }

        fwrite(addr,length,1,fp);
        usleep(500);

        fclose(fp);

        return 0;
}

int read_frame(int fd)
{
        printf("Read frame start ---> n");
        struct v4l2_buffer buf;
        unsigned int i;

        bzero(&buf,sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
      
printf("Read frame end \n");
  
//Get buf from queue
        if(-1 == ioctl(fd,VIDIOC_DQBUF,&buf))
        {
                perror("Fail to ioctl 'VIDIOC_DQBUF'");
                exit(EXIT_FAILURE);
        }

        assert(buf.index < n_buffer);
       
//read the process space's data to another file
        //process_image(user_buf[buf.index].start,user_buf[buf.index].length);

        CvMat cvmat = cvMat(480, 640, CV_8UC3, (void*)buf);
        IplImage * img;
        img = cvDecodeImage(&cvmat, 1);

        imwrite( "Image.jpg", img );

        if(-1 == ioctl(fd,VIDIOC_QBUF,&buf))
        {
                perror("Fail to ioctl 'VIDIOC_QBUF'");
                exit(EXIT_FAILURE);
        }

        return 1;
}

int mainloop(int fd)
{
        int count = 5;

        while(count -- > 0)
        {
                for(;;)
                {
                        fd_set fds;
                        struct timeval tv;
                        int r;

                        FD_ZERO(&fds);
                        FD_SET(fd,&fds);

                        /*Timeout*/
                        tv.tv_sec = 2;
                        tv.tv_usec = 0;

                        r = select(fd + 1,&fds,NULL,NULL,&tv);

                        if(-1 == r)
                        {
                                if(EINTR == errno)
                                        continue;

                                perror("Fail to select");
                                exit(EXIT_FAILURE);
                        }

                        if(0 == r)
                        {
                                fprintf(stderr,"select Timeout\n");
                                exit(EXIT_FAILURE);
                        }

                        if(read_frame(fd))
                          break;
                }
        }

        return 0;
}

void stop_capturing(int fd)
{
        printf("Stop capturing \n");
        enum v4l2_buf_type type;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(-1 == ioctl(fd,VIDIOC_STREAMOFF,&type))
        {
                perror("Fail to ioctl 'VIDIOC_STREAMOFF'");
                exit(EXIT_FAILURE);
        }

        return;
}

void uninit_camer_device()
{
        unsigned int i;

        for(i = 0;i < n_buffer;i ++)
        {
                if(-1 == munmap(user_buf[i].start,user_buf[i].length))
                {
                        exit(EXIT_FAILURE);
                }
        }

        free(user_buf);

        return;
}

void close_camer_device(int fd)
{
        if(-1 == close(fd))
        {
                perror("Fail to close fd");
                exit(EXIT_FAILURE);
        }

        return;
}

int main()
{
        int fd;      

        fd = open_camer_device();

        init_camer_device(fd);

        start_capturing(fd);

        mainloop(fd);

        stop_capturing(fd);

        uninit_camer_device();

        close_camer_device(fd);

        return 0;
}


