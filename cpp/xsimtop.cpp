/* Copyright (c) 2014 Quanta Research Cambridge, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <queue>

#include "xsi_loader.h"

#include <portal.h>
#include <sock_utils.h>
#include <GeneratedTypes.h>
#include <XsimMemSlaveRequest.h>
#include <XsimMemSlaveIndication.h>

XsimMemSlaveIndicationProxy *memSlaveIndicationProxy;
class XsimMemSlaveRequest;
XsimMemSlaveRequest *memSlaveRequest;
static int trace_xsimtop = 1;

//deleteme
std::string getcurrentdir()
{
    return get_current_dir_name();
}

class xsiport {
  Xsi::Loader &xsiInstance;
  int port;
  //int direction;
  const char *name;
  int width;
  s_xsi_vlog_logicval value;
public:
  xsiport(Xsi::Loader &loader, const char *name, int bits = 1)
    : xsiInstance(loader), port(-1), name(name), width(bits)
  {
    value.aVal = 1;
    value.bVal = 1;
    port = xsiInstance.get_port_number(name);
    //width = xsiInstance.get_int_port(port, xsiHDLValueSize);
    std::cout << "Port name=" << name << " number=" << port << std::endl;
  }
  int read();
  void write(int val);
  int valid() { return port >= 0; }
};

int xsiport::read()
{
    xsiInstance.get_value(port, &value);
    int mask = (width == 32) ? -1 : ((1 << width) - 1);
    if (value.bVal != 0) {
      char charval[] = { '0', '1', 'Z', 'X' };
      int encoding = (value.aVal & mask) | ((value.bVal & mask) << 1);
      fprintf(stderr, "port %2d.%16s value=%08x.%08x mask=%08x width=%2d %c\n", port, name, value.aVal, value.bVal, mask, width, charval[encoding]);
    }
    return value.aVal & mask;
}
void xsiport::write(int aVal)
{
    value.aVal = aVal;
    value.bVal = 0;
    xsiInstance.put_value(port, &value);
}

class XsimMemSlaveRequest : public XsimMemSlaveRequestWrapper {
  struct idInfo {
    int number;
    int id;
    int valid;
  } ids[16];
  int portal_count;
public:
  struct readreq {
    uint32_t addr;
  };
  struct writereq {
    uint32_t addr;
    uint32_t data;
  };
  std::queue<readreq> readreqs;
  std::queue<uint32_t> readdata;
  std::queue<writereq> writereqs;
  std::queue<uint32_t> sinkbeats;

  int connected;

  XsimMemSlaveRequest(int id, PortalItemFunctions *item, void *param, PortalPoller *poller = 0) : XsimMemSlaveRequestWrapper(id, item, param, poller), connected(0) { }
  ~XsimMemSlaveRequest() {}
  virtual void connect () {
      connected = 1;
  }
  virtual void enableint( const uint32_t fpgaId, const uint8_t val);
  virtual void read ( const uint32_t fpgaId, const uint32_t addr );
  virtual void write ( const uint32_t fpgaId, const uint32_t addr, const uint32_t data );
  virtual void msgSink ( const uint32_t data );

  void directory( const uint32_t fpgaNumber, const uint32_t fpgaId, const uint32_t last );
  int fpgaNumber(int fpgaId);
  int fpgaId(int fpgaNumber);

};

void XsimMemSlaveRequest::enableint( const uint32_t fpgaId, const uint8_t val)
{
  int number = fpgaNumber(fpgaId);
  uint32_t hwaddr = number << 16 | 4;
  writereq req = { hwaddr, val };
  fprintf(stderr, "[%s:%d] id=%d number=%d addr=%08x\n", __FUNCTION__, __LINE__, fpgaId, number, hwaddr);
  writereqs.push(req);
}

void XsimMemSlaveRequest::read ( const uint32_t fpgaId, const uint32_t addr )
{
  int number = fpgaNumber(fpgaId);
  fprintf(stderr, "[%s:%d] id=%d number=%d addr=%08x\n", __FUNCTION__, __LINE__, fpgaId, number, addr);
  uint32_t hwaddr = number << 16 | addr;
  readreq req = { hwaddr };
  readreqs.push(req);
}

void XsimMemSlaveRequest::write ( const uint32_t fpgaId, const uint32_t addr, const uint32_t data )
{
  int number = fpgaNumber(fpgaId);
  uint32_t hwaddr = number << 16 | addr;
  writereq req = { hwaddr, data };
  fprintf(stderr, "[%s:%d] id=%d number=%d addr=%08x/%08x data=%08x\n", __FUNCTION__, __LINE__, fpgaId, fpgaNumber(fpgaId), addr, hwaddr, data);
  writereqs.push(req);
}

void XsimMemSlaveRequest::msgSink ( const uint32_t data )
{
  if (trace_xsimtop)
  fprintf(stderr, "[%s:%d] data=%08x\n", __FUNCTION__, __LINE__, data);
  sinkbeats.push(data);
}

void XsimMemSlaveRequest::directory ( const uint32_t fpgaNumber, const uint32_t fpgaId, const uint32_t last )
{
    fprintf(stderr, "[%s:%d] fpga=%d id=%d last=%d\n", __FUNCTION__, __LINE__, fpgaNumber, fpgaId, last);
    struct idInfo info = { fpgaNumber, fpgaId, 1 };
    ids[fpgaNumber] = info;
    if (last)
      portal_count = fpgaNumber+1;
}

int XsimMemSlaveRequest::fpgaNumber(int fpgaId)
{
    for (int i = 0; ids[i].valid; i++)
	if (ids[i].id == fpgaId) {
	    return ids[i].number;
	}

    PORTAL_PRINTF( "Error: %s: did not find fpga_number %d\n", __FUNCTION__, fpgaId);
    PORTAL_PRINTF( "    Found fpga numbers:");
    for (int i = 0; ids[i].valid; i++)
	PORTAL_PRINTF( " %d", ids[i].id);
    PORTAL_PRINTF( "\n");

    return 0;
}
int XsimMemSlaveRequest::fpgaId(int fpgaNumber)
{
  return ids[fpgaNumber].id;
}

enum xsimtop_state {
  xt_reset, xt_read_directory, xt_active
};

int main(int argc, char **argv)
{
    std::string cwd = getcurrentdir();
    std::string simengine_libname = "librdi_simulator_kernel.so";
    std::string design_libname = getcurrentdir() + "/xsim.dir/mkXsimTop/xsimk.so";

    std::cout << "Design DLL     : " << design_libname << std::endl;
    std::cout << "Sim Engine DLL : " << simengine_libname << std::endl;

    // See xsi.h header for more details on how Verilog values are stored as aVal/bVal pairs
    Xsi::Loader xsiInstance(design_libname, simengine_libname);
    s_xsi_setup_info info;
    info.logFileName = (char *)"xsim.log";
    xsiInstance.open(&info);
    
    xsimtop_state state = xt_reset;

    xsiport rst_n(xsiInstance, "RST_N");
    xsiport clk(xsiInstance, "CLK");

    xsiport msgSource_src_rdy(xsiInstance, "msgSource_src_rdy");
    xsiport msgSource_dst_rdy_b(xsiInstance, "msgSource_dst_rdy_b");
    xsiport msgSource_beat(xsiInstance, "msgSource_beat", 32);
    xsiport msgSink_dst_rdy(xsiInstance, "msgSink_dst_rdy");
    xsiport msgSink_src_rdy_b(xsiInstance, "msgSink_src_rdy_b");
    xsiport msgSink_beat_v(xsiInstance, "msgSink_beat_v", 32);

    PortalSocketParam paramSocket = {};
    PortalMuxParam param = {};

    if (msgSource_beat.valid())
	fprintf(stderr, "[%s:%d] using BluenocTop\n", __FILE__, __LINE__);
    Portal *mcommon = new Portal(0, 0, sizeof(uint32_t), portal_mux_handler, NULL, &socketfuncResp, &paramSocket);
    param.pint = &mcommon->pint;
    XsimMemSlaveIndicationProxy *memSlaveIndicationProxy = new XsimMemSlaveIndicationProxy(XsimIfcNames_XsimMemSlaveIndication, &muxfunc, &param);
    XsimMemSlaveRequest *memSlaveRequest = new XsimMemSlaveRequest(XsimIfcNames_XsimMemSlaveRequest, &muxfunc, &param);

    printf("[%s:%d]\n", __FUNCTION__, __LINE__);
    //portalExec_init();
    portalExec_stop();
    sleep(2);
    printf("[%s:%d]\n", __FUNCTION__, __LINE__);

    // start low clock
    clk.write(0);
    rst_n.write(0);

    xsiInstance.run(10000);

    int portal_number = 0;
    int waiting_for_read = 0;
    int read_dir_val = 0;
    int portal_ids[16];
    int portal_count = 0;
    int offset = 0x00;

    for (int ind = 0; 1; ind++) {
	void *rc = portalExec_poll(1);
	if ((long)rc >= 0) {
	    portalExec_event();
	}
	if (ind > 2) {
	    rst_n.write(1);
	}
	// mkBluenocTop
	//fprintf(stderr, "msgSource ready %d msgSink ready %d\n", msgSource_src_rdy.read(), msgSink_dst_rdy.read());
	if (msgSource_src_rdy.read()) {
	    uint32_t beat = msgSource_beat.read();
	    msgSource_dst_rdy_b.write(1);
	    if (trace_xsimtop)
		fprintf(stderr, "[%s:%d] source message beat %08x\n", __FUNCTION__, __LINE__, beat);
	    memSlaveIndicationProxy->msgSource(beat);
	} else {
	    msgSource_dst_rdy_b.write(0);
	}

	if (msgSink_dst_rdy.read() && memSlaveRequest->sinkbeats.size()) {
	    uint32_t beat = memSlaveRequest->sinkbeats.front();
	    memSlaveRequest->sinkbeats.pop();

	    if (trace_xsimtop)
		fprintf(stderr, "[%s:%d] sink message beat %08x\n", __FUNCTION__, __LINE__, beat);
	    msgSink_beat_v.write(beat);
	    msgSink_src_rdy_b.write(1);

	} else {
	    msgSink_src_rdy_b.write(0);
	}

	// setup time
	xsiInstance.run(10000);
	clk.write(1);
	xsiInstance.run(10000);
	if (0) {
	    // clock is divided by two
	    clk.write(0);
	    xsiInstance.run(10);
	    clk.write(1);
	    xsiInstance.run(10);
	}
	clk.write(0);
	xsiInstance.run(10000);
    }
    sleep(10);
    //portalExec_end();
}
