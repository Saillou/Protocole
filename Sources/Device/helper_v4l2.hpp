#pragma once
#ifdef __linux__

// Use v4l2
#include <linux/videodev2.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>

namespace hvl {
	// ---------- Tools ----------
	// Print error with errno
	void printError(int fd, const std::string& message) {
		std::string mes = "[" + std::to_string(fd) + "] " + message + " - Errno: " +  std::to_string(errno);
		perror(mes.c_str());	
	}
	
	// Make a ioctl request 
	int xioctl(int fd, int request, void *arg) {
		int r(-1);
		do {
			r = ioctl (fd, request, arg);
		} while (r == -1 && errno == EINTR);
		
		return r;	
	}
	
	// Combine ioctl and perror
	bool ioctlAct(int fd, int request, void *arg, const std::string& errorMsg = "") {
		if(xioctl(fd, request, arg) == -1) {
			printError(fd, errorMsg);
			return false;
		}	
		return true;
	}
	
	// ---------- V4l2 commands ----------
	// --- File descriptor ---
	// Open file descriptor in non blocking mdoe
	bool openfd(int& fd, const std::string& path) {
		fd = -1;
		fd = open(path.c_str(), O_RDWR | O_NONBLOCK);

		if(fd == -1) {
			printError(fd, "Opening file descriptor");
			return false;
		}
		return true;
	}
	
	// Close file descriptor
	bool closefd(int& fd) {
		if(close(fd) == -1) {
			printError(fd, "Closing file descriptor");
			return false;
		}
		
		fd = -1;
		return true;		
	}
	
	
	// --- Capture ---
	// Start capture during memory mapping
	bool startCapture(int fd) {
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		
		return ioctlAct(fd, VIDIOC_STREAMON,  &type, "Starting Capture");		
	}
	
	// Stop capture during memory mapping
	bool stopCapture(int fd) {
		enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		
		return ioctlAct(fd, VIDIOC_STREAMOFF,  &type, "Stopping Capture");
	}
	
	
	// --- Buffers  ---
	// Allocate 1 buffer (once allocate buffer can be queued/unqueued/cheked)
	bool requestBuffer(int fd) {
		struct v4l2_requestbuffers req = {0};

		req.count 	= 1;
		req.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		req.memory 	= V4L2_MEMORY_MMAP;
		
		return ioctlAct(fd, VIDIOC_REQBUFS,  &req, "Requesting Buffer");	
	}
	
	// Desallocate any buffers
	bool freeBuffer(int fd) {
		struct v4l2_requestbuffers req = {0};

		req.count 	= 0;
		req.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		req.memory 	= V4L2_MEMORY_MMAP;
		
		return ioctlAct(fd, VIDIOC_REQBUFS,  &req, "Freeing Buffer");
	}
	
	// Check status of buffer (use for querying the size)
	bool queryBuffer(int fd, struct v4l2_buffer& buf) {
		buf.index 	= 0;
		buf.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory 	= V4L2_MEMORY_MMAP;
		
		return ioctlAct(fd, VIDIOC_QUERYBUF,  &buf, "Querying Buffer");
	}
	
	// Queue an empty buffer (when filled, use dequeue to get it)
	bool queueBuffer(int fd) {
		struct v4l2_buffer buf = {0};
		buf.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory 	= V4L2_MEMORY_MMAP;
		
		return ioctlAct(fd, VIDIOC_QBUF,  &buf, "Queuing Buffer");
	}
	
	// Unqueue a filled buffer (set errno to EINVAL if nothing ready yet)
	bool dequeueBuffer(int fd, struct v4l2_buffer& buf) {
		buf.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory 	= V4L2_MEMORY_MMAP;
		
		return ioctlAct(fd, VIDIOC_DQBUF,  &buf, "Dequeuing Buffer");
	}
	
	
	// --- Memory ---
	// Link v4l2 buffer to our buffer
	bool memoryMap(int fd, struct v4l2_buffer& vlBuf, void** pBufStart, size_t& bufLen) {
		*pBufStart 	= mmap(nullptr, vlBuf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, vlBuf.m.offset);
		bufLen 		= vlBuf.bytesused > 0 ? vlBuf.bytesused : vlBuf.length;
		
		if(*pBufStart == MAP_FAILED) {
			printError(fd, "Memory map");
			return false;
		}
		return true;
	}
	
	// Remove memory link
	bool memoryUnmap(int fd, void** pBufStart, size_t& bufLen) {
		struct v4l2_buffer buf = {0};
		if(!queryBuffer(fd, buf))
			return false;
		
		if(munmap(*pBufStart, buf.length) == -1) {
			printError(fd, "Memory unmap");
			return false;
		}
		
		*pBufStart = nullptr;
		bufLen = 0;
		return true;
	}
	
	
	// --- Format ---
	bool setFormat(int fd, int width, int height) {
		struct v4l2_format fmt = {0};	
		fmt.type 						= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.pixelformat 	= V4L2_PIX_FMT_MJPEG;
		fmt.fmt.pix.field 			= V4L2_FIELD_ANY;
		fmt.fmt.pix.width 			= width;
		fmt.fmt.pix.height 			= height;
		
		return ioctlAct(fd, VIDIOC_S_FMT,  &fmt, "Setting format");
	}
	
	
	// --- Controls ---
	// Check control capabilities (maximum, minimum, default value)
	bool queryControl(int fd, int id, struct v4l2_queryctrl* pqCtrl) {
		pqCtrl->id = id;
		ioctlAct(fd, VIDIOC_G_CTRL, pqCtrl, "Querying control");
		std::cout << "query: " << pqCtrl->maximum << std::endl;
		return true;
		// return ioctlAct(fd, VIDIOC_G_CTRL, pqCtrl, "Querying control");
	}
	
	// Change control value
	bool setControl(int fd, struct v4l2_control* pCtrl) {
		return ioctlAct(fd, VIDIOC_S_CTRL,  pCtrl, "Setting control");
	}
	
	// Get control value
	bool getControl(int fd, struct v4l2_control* pCtrl) {
		return ioctlAct(fd, VIDIOC_G_CTRL, pCtrl, "Getting control");		
	}
}

#endif