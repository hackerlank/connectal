
// Copyright (c) 2012 Nokia, Inc.

// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies
// of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
// BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <errno.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#ifdef ZYNQ
#include <android/log.h>
#endif

#include "portal.h"
#include "sock_utils.h"
#include "sock_fd.h"

#ifdef ZYNQ
#define ALOGD(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, "PORTAL", fmt, __VA_ARGS__)
#define ALOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, "PORTAL", fmt, __VA_ARGS__)
#else
#define ALOGD(fmt, ...) fprintf(stderr, "PORTAL: " fmt, __VA_ARGS__)
#define ALOGE(fmt, ...) fprintf(stderr, "PORTAL: " fmt, __VA_ARGS__)
#endif


PortalWrapper **portal_wrappers = 0;
struct pollfd *portal_fds = 0;
int numFds = 0;

void Portal::close()
{
    if (fd > 0) {
        ::close(fd);
        fd = -1;
    }    
}

Portal::Portal(const char *devname, unsigned int addrbits)
  : fd(-1),
    ind_reg_base(0x0), 
    ind_fifo_base(0x0),
    req_reg_base(0x0),
    req_fifo_base(0x0),
    name(strdup(devname))
{
  int rc = open(addrbits);
  if (rc != 0) {
    printf("[%s:%d] failed to open Portal %s\n", __FUNCTION__, __LINE__, name);
    ALOGD("Portal::Portal failure rc=%d\n", rc);
    exit(1);
  }
}
Portal::Portal(int id)
  : fd(-1),
    ind_reg_base(0x0), 
    ind_fifo_base(0x0),
    req_reg_base(0x0),
    req_fifo_base(0x0)
{
  assert(false);
}

Portal::~Portal()
{
  close();
  free(name);
}



int Portal::open(int length)
{
#ifdef ZYNQ
    FILE *pgfile = fopen("/sys/devices/amba.0/f8007000.devcfg/prog_done", "r");
    if (!pgfile) {
        // 3.9 kernel uses amba.2
        pgfile = fopen("/sys/devices/amba.2/f8007000.devcfg/prog_done", "r");
    }
    if (pgfile == 0) {
	ALOGE("failed to open /sys/devices/amba.[02]/f8007000.devcfg/prog_done %d\n", errno);
	printf("failed to open /sys/devices/amba.[02]/f8007000.devcfg/prog_done %d\n", errno);
	return -1;
    }
    char line[128];
    fgets(line, sizeof(line), pgfile);
    if (line[0] != '1') {
	ALOGE("FPGA not programmed: %s\n", line);
	printf("FPGA not programmed: %s\n", line);
	return -ENODEV;
    }
    fclose(pgfile);
#endif
#ifdef MMAP_HW

    char path[128];
    snprintf(path, sizeof(path), "/dev/%s", name);
#ifdef ZYNQ
    this->fd = ::open(path, O_RDWR);
#else
    // FIXME: bluenoc driver only opens readonly for some reason
    this->fd = ::open(path, O_RDONLY);
#endif
    if (this->fd < 0) {
	ALOGE("Failed to open %s fd=%d errno=%d\n", path, this->fd, errno);
	return -errno;
    }
    volatile unsigned int *dev_base = (volatile unsigned int*)mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, this->fd, 0);
    if (dev_base == MAP_FAILED) {
      ALOGE("Failed to mmap PortalHWRegs from fd=%d errno=%d\n", this->fd, errno);
      return -errno;
    }  
    ind_reg_base   = (volatile unsigned int*)(((unsigned long)dev_base)+(3<<14));
    ind_fifo_base  = (volatile unsigned int*)(((unsigned long)dev_base)+(2<<14));
    req_reg_base   = (volatile unsigned int*)(((unsigned long)dev_base)+(1<<14));
    req_fifo_base  = (volatile unsigned int*)(((unsigned long)dev_base)+(0<<14));

    // enable interrupts
    fprintf(stderr, "Portal::enabling interrupts %s\n", name);
    *(ind_reg_base+0x1) = 1;

#else
    snprintf(p.read.path, sizeof(p.read.path), "%s_rc", name);
    connect_socket(&(p.read));
    snprintf(p.write.path, sizeof(p.read.path), "%s_wc", name);
    connect_socket(&(p.write));

    unsigned long dev_base = 0;
    ind_reg_base   = dev_base+(3<<14);
    ind_fifo_base  = dev_base+(2<<14);
    req_reg_base   = dev_base+(1<<14);
    req_fifo_base  = dev_base+(0<<14);
#endif
    return 0;
}

int Portal::sendMessage(PortalMessage *msg)
{

  // TODO: this intermediate buffer (and associated copy) should be removed (mdk)
  unsigned int buf[128];
  msg->marshall(buf);

  // mutex_lock(&portal_data->reg_mutex);
  // mutex_unlock(&portal_data->reg_mutex);
  for (int i = msg->size()/4-1; i >= 0; i--) {
    unsigned int data = buf[i];
#ifdef MMAP_HW
    unsigned long addr = ((unsigned long)req_fifo_base) + msg->channel * 256;
    //fprintf(stderr, "%08lx %08x\n", addr, data);
    *((volatile unsigned int*)addr) = data;
#else
    unsigned int addr = req_fifo_base + msg->channel * 256;
    struct memrequest foo = {true,addr,data};
    if (send(p.write.s2, &foo, sizeof(foo), 0) == -1) {
      fprintf(stderr, "%s (%s) send error\n",__FUNCTION__, name);
      exit(1);
    }
    //fprintf(stderr, "(%s) sendMessage\n", name);
#endif
  }
  return 0;
}

PortalWrapper::PortalWrapper(int id) 
  : Portal(id)
{
  registerInstance();
}

PortalWrapper::PortalWrapper(const char *devname, unsigned int addrbits)
  : Portal(devname,addrbits)
{
}

PortalWrapper::~PortalWrapper()
{
  unregisterInstance();
}

PortalProxy::PortalProxy(int id)
  : Portal(id)
{
}

PortalProxy::PortalProxy(const char *devname, unsigned int addrbits)
  : Portal(devname,addrbits)
{
}

PortalProxy::~PortalProxy()
{
}

int PortalWrapper::unregisterInstance()
{
  int i = 0;
  while(i < numFds)
    if(portal_fds[i].fd == this->fd)
      break;
    else
      i++;

  while(i < numFds-1){
    portal_fds[i] = portal_fds[i+1];
    portal_wrappers[i] = portal_wrappers[i+1];
  }

  numFds--;
  portal_fds = (struct pollfd *)realloc(portal_fds, numFds*sizeof(struct pollfd));
  portal_wrappers = (PortalWrapper **)realloc(portal_wrappers, numFds*sizeof(PortalWrapper *));  
  return 0;
}

int PortalWrapper::registerInstance()
{
    numFds++;
    portal_wrappers = (PortalWrapper **)realloc(portal_wrappers, numFds*sizeof(PortalWrapper *));
    portal_wrappers[numFds-1] = this;
    portal_fds = (struct pollfd *)realloc(portal_fds, numFds*sizeof(struct pollfd));
    struct pollfd *pollfd = &portal_fds[numFds-1];
    memset(pollfd, 0, sizeof(struct pollfd));
    pollfd->fd = this->fd;
    pollfd->events = POLLIN;
    return 0;
}

PortalMemory::PortalMemory(const char *devname, unsigned int addrbits)
  : PortalProxy(devname, addrbits)
  , handle(1)
{
#ifndef MMAP_HW
  snprintf(p_fd.read.path, sizeof(p_fd.read.path), "fd_sock_rc");
  connect_socket(&(p_fd.read));
  snprintf(p_fd.write.path, sizeof(p_fd.write.path), "fd_sock_wc");
  connect_socket(&(p_fd.write));
#endif
  const char* path = "/dev/portalmem";
  this->pa_fd = ::open(path, O_RDWR);
  if (this->pa_fd < 0){
    ALOGE("Failed to open %s pa_fd=%ld errno=%d\n", path, (long)this->pa_fd, errno);
  }
}

PortalMemory::PortalMemory(int id)
  : PortalProxy(id),
    handle(1)
{
  assert(false);
}

void *PortalMemory::mmap(PortalAlloc *portalAlloc)
{
  void *virt = ::mmap(0, portalAlloc->header.size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_SHARED, portalAlloc->header.fd, 0);
  return virt;
}

int PortalMemory::dCacheFlushInval(PortalAlloc *portalAlloc, void *__p)
{
#if defined(__arm__)
  int rc = ioctl(this->pa_fd, PA_DCACHE_FLUSH_INVAL, portalAlloc);
  if (rc){
    fprintf(stderr, "portal dcache flush failed rc=%d errno=%d:%s\n", rc, errno, strerror(errno));
    return rc;
  }
#elif defined(__i386__) || defined(__x86_64__)
  // not sure any of this is necessary (mdk)
  for(int i = 0; i < portalAlloc->header.size; i++){
    char foo = *(((volatile char *)__p)+i);
    asm volatile("clflush %0" :: "m" (foo));
  }
  asm volatile("mfence");
#else
#error("dCAcheFlush not defined for unspecified architecture")
#endif
  fprintf(stderr, "dcache flush\n");
  return 0;

}

int PortalMemory::reference(PortalAlloc* pa)
{
  int id = handle++;
#ifdef MMAP_HW
  int ne = pa->header.numEntries;
  assert(ne < 32);
  pa->entries[ne].dma_address = 0;
  pa->entries[ne].length = 0;
  pa->header.numEntries;
  for(int i = 0; i <= pa->header.numEntries; i++){
    assert(i<32); // the HW has defined SGListMaxLen as 32
    fprintf(stderr, "PortalMemory::sglist(%08x, %08lx, %08lx)\n", id, pa->entries[i].dma_address, pa->entries[i].length);
    sglist(id, pa->entries[i].dma_address, pa->entries[i].length);
    sleep(1); // ugly hack.  should use a semaphore for flow-control (mdk)
  }
#else
  sock_fd_write(p_fd.write.s2, pa->header.fd);
  paref(id, pa->header.size);
  sleep(1); // ugly hack.  should use a semaphore for flow-control (mdk)
#endif
  return id;
}

int PortalMemory::alloc(size_t size, PortalAlloc **ppa)
{
  PortalAlloc *portalAlloc = (PortalAlloc *)malloc(sizeof(PortalAlloc));
  memset(portalAlloc, 0, sizeof(PortalAlloc));
  portalAlloc->header.size = size;
  int rc = ioctl(this->pa_fd, PA_ALLOC, portalAlloc);
  if (rc){
    fprintf(stderr, "portal alloc failed rc=%d errno=%d:%s\n", rc, errno, strerror(errno));
    return rc;
  }
  fprintf(stderr, "alloc size=%ld rc=%d fd=%d numEntries=%d\n", 
	  (long)portalAlloc->header.size, rc, portalAlloc->header.fd, portalAlloc->header.numEntries);
  portalAlloc = (PortalAlloc *)realloc(portalAlloc, sizeof(PortalAlloc)+(portalAlloc->header.numEntries*sizeof(DMAEntry)));
  rc = ioctl(this->pa_fd, PA_DMA_ADDRESSES, portalAlloc);
  if (rc){
    fprintf(stderr, "portal alloc failed rc=%d errno=%d:%s\n", rc, errno, strerror(errno));
    return rc;
  }
  *ppa = portalAlloc;
  return 0;
}

int Portal::setClockFrequency(int clkNum, long requestedFrequency, long *actualFrequency)
{
    if (!numFds) {
	ALOGE("%s No fds open\n", __FUNCTION__);
	return -ENODEV;
    }

    PortalClockRequest request;
    request.clknum = clkNum;
    request.requested_rate = requestedFrequency;
    int status = ioctl(portal_fds[0].fd, PORTAL_SET_FCLK_RATE, (long)&request);
    if (status == 0 && actualFrequency)
	*actualFrequency = request.actual_rate;
    if (status < 0)
	status = errno;
    return status;
}

void* portalExec(void* __x)
{
#ifdef MMAP_HW
    long rc;
    int timeout = -1;
#ifndef ZYNQ
    timeout = 1; // interrupts not working yet on PCIe
#endif
    if (!numFds) {
        ALOGE("portalExec No fds open numFds=%d\n", numFds);
        return (void*)-ENODEV;
    }
    while ((rc = poll(portal_fds, numFds, timeout)) >= 0) {
      if (0)
	fprintf(stderr, "poll returned rc=%ld\n", rc);
#ifndef ZYNQ
      // PCIE interrupts not working
      if (1)
	{
	  PortalWrapper *instance = portal_wrappers[0];
	  unsigned int int_src = *(volatile int *)(instance->ind_reg_base+0x0);
	  unsigned int int_en  = *(volatile int *)(instance->ind_reg_base+0x1);
	  unsigned int ind_count  = *(volatile int *)(instance->ind_reg_base+0x2);
	  unsigned int queue_status = *(volatile int *)(instance->ind_reg_base+0x8);
	  if (0)
	    fprintf(stderr, "%d: int_src=%08x int_en=%08x ind_count=%08x queue_status=%08x\n",
		    __LINE__, int_src, int_en, ind_count, queue_status);
	}
#endif      
      for (int i = 0; i < numFds; i++) {
#ifndef ZYNQ
	// PCIE interrupts not working
	if (0)
#endif
	if (!portal_wrappers) {
	  fprintf(stderr, "No portal_instances but rc=%ld revents=%d\n", rc, portal_fds[i].revents);
	}
	
	PortalWrapper *instance = portal_wrappers[i];
	
	// sanity check, to see the status of interrupt source and enable
	unsigned int int_src = *(volatile int *)(instance->ind_reg_base+0x0);
	unsigned int int_en  = *(volatile int *)(instance->ind_reg_base+0x1);
	unsigned int ind_count  = *(volatile int *)(instance->ind_reg_base+0x2);
	unsigned int queue_status = *(volatile int *)(instance->ind_reg_base+0x8);
	if(0)
	  fprintf(stderr, "(%d) about to receive messages %08x %08x %08x\n", i, int_src, int_en, queue_status);

	// handle all messasges from this portal instance
	while (queue_status) {
	  if(0)
	    fprintf(stderr, "queue_status %d\n", queue_status);
	  instance->handleMessage(queue_status-1);
	  int_src = *(volatile int *)(instance->ind_reg_base+0x0);
	  int_en  = *(volatile int *)(instance->ind_reg_base+0x1);
	  ind_count  = *(volatile int *)(instance->ind_reg_base+0x2);
	  queue_status = *(volatile int *)(instance->ind_reg_base+0x8);
	  if (0)
	    fprintf(stderr, "%d: int_src=%08x int_en=%08x ind_count=%08x queue_status=%08x\n",
		    __LINE__, int_src, int_en, ind_count, queue_status);
	}
	
	// rc of 0 indicates timeout
	if (rc == 0) {
	  // do something if we timeout??
	}
	// re-enable interupt which was disabled by portal_isr
	*(instance->ind_reg_base+0x1) = 1;
      }
    }
    // return only in error case
    fprintf(stderr, "poll returned rc=%ld errno=%d:%s\n", rc, errno, strerror(errno));
    return (void*)rc;
#else // BSIM
    fprintf(stderr, "about to enter while(true)\n");
    while (true){
      sleep(0);
      for(int i = 0; i < numFds; i++){
	PortalWrapper *instance = portal_wrappers[i];
	unsigned int addr = instance->ind_reg_base+0x20;
	struct memrequest foo = {false,addr,0};
	//fprintf(stderr, "sending read request\n");
	if (send(instance->p.read.s2, &foo, sizeof(foo), 0) == -1) {
	  fprintf(stderr, "%s (%s) send error\n",__FUNCTION__, instance->name);
	  exit(1);
	}
	unsigned int queue_status;
	//fprintf(stderr, "about to get read response\n");
	if(recv(instance->p.read.s2, &queue_status, sizeof(queue_status), 0) == -1){
	  fprintf(stderr, "portalExec recv error (%s)\n", instance->name);
	  exit(1);	  
	}
	//fprintf(stderr, "(%s) queue_status : %08x\n", instance->name, queue_status);
	if (queue_status){
	  instance->handleMessage(queue_status-1);	
	}
      }
    }
#endif
}

