// bsv libraries
import SpecialFIFOs::*;
import Vector::*;
import StmtFSM::*;
import FIFO::*;

// portz libraries
import AxiClientServer::*;
import AxiMasterSlave::*;
import Directory::*;
import CtrlMux::*;
import Portal::*;
import Leds::*;
import BlueScope::*;
import PortalMemory::*;
import PortalRMemory::*;
`ifdef BSIM
import BsimRDMA::*;
`else
import AxiRDMA::*;
`endif

// generated by tool
import MemcpyRequestWrapper::*;
import BlueScopeRequestWrapper::*;
import DMARequestWrapper::*;
import MemcpyIndicationProxy::*;
import BlueScopeIndicationProxy::*;
import DMAIndicationProxy::*;

// defined by user
import Memcpy::*;

module mkPortalTop(StdPortalDmaTop);

   DMAIndicationProxy dmaIndicationProxy <- mkDMAIndicationProxy(9);
   // Max burst 16
   DMAReadBuffer#(64,8) dma_stream_read_chan <- mkDMAReadBuffer();

   // Max burst 16
   DMAWriteBuffer#(64,8) dma_stream_write_chan <- mkDMAWriteBuffer();
   
   // Max burst 1 because it only reads one word at a time
   DMAReadBuffer#(64,1) dma_word_read_chan <- mkDMAReadBuffer();

   // Max burst 16
   DMAWriteBuffer#(64,16) dma_debug_write_chan <- mkDMAWriteBuffer();

   Vector#(2,  DMAReadClient#(64))   readClients = newVector();
   readClients[0] = dma_stream_read_chan.dmaClient;
   readClients[1] = dma_word_read_chan.dmaClient;

   Vector#(2, DMAWriteClient#(64)) writeClients = newVector();
   writeClients[0] = dma_stream_write_chan.dmaClient;
   writeClients[1] = dma_debug_write_chan.dmaClient;

`ifdef BSIM
   BsimDMAServer#(64)     dma <- mkBsimDMAServer(dmaIndicationProxy.ifc, readClients, writeClients);
`else
   Integer               numRequests = 8;
   AxiDMAServer#(64,8)   dma <- mkAxiDMAServer(dmaIndicationProxy.ifc, numRequests, readClients, writeClients);
`endif

   DMARequestWrapper dmaRequestWrapper <- mkDMARequestWrapper(1005,dma.request);

   BlueScopeIndicationProxy blueScopeIndicationProxy <- mkBlueScopeIndicationProxy(8);
   BlueScope#(64) bs <- mkBlueScope(32, dma_debug_write_chan.dmaServer, blueScopeIndicationProxy.ifc);
   BlueScopeRequestWrapper blueScopeRequestWrapper <- mkBlueScopeRequestWrapper(1003,bs.requestIfc);

   MemcpyIndicationProxy memcpyIndicationProxy <- mkMemcpyIndicationProxy(7);
   MemcpyRequest memcpyRequest <- mkMemcpyRequest(memcpyIndicationProxy.ifc, dma_stream_read_chan.dmaServer,
						  dma_stream_write_chan.dmaServer, dma_word_read_chan.dmaServer, bs);
   MemcpyRequestWrapper memcpyRequestWrapper <- mkMemcpyRequestWrapper(1008,memcpyRequest);

   Vector#(6,StdPortal) portals;
   portals[0] = memcpyRequestWrapper.portalIfc;
   portals[1] = memcpyIndicationProxy.portalIfc; 
   portals[2] = blueScopeRequestWrapper.portalIfc;
   portals[3] = blueScopeIndicationProxy.portalIfc; 
   portals[4] = dmaRequestWrapper.portalIfc;
   portals[5] = dmaIndicationProxy.portalIfc; 
   
   Directory dir <- mkDirectory(portals);
   Vector#(1,StdPortal) directories;
   directories[0] = dir.portalIfc;
   
   // when constructing ctrl and interrupt muxes, directories must be the first argument
   let ctrl_mux <- mkAxiSlaveMux(directories,portals);
   let interrupt_mux <- mkInterruptMux(portals);
   
   interface ReadOnly interrupt = interrupt_mux;
   interface StdAxi3Slave ctrl = ctrl_mux;
`ifndef BSIM
   interface Vector m_axi = replicate(dma.m_axi);
`else
   interface Vector m_axi = ?;
`endif
   interface LEDS leds = ?;
endmodule


