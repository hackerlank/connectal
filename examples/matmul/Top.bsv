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
import SpecialFIFOs::*;
import Vector::*;
import StmtFSM::*;
import FIFO::*;
import Connectable::*;
import CtrlMux::*;
import Portal::*;
import HostInterface::*;
import ConnectalMemory::*;
import MemTypes::*;
import MemServer::*;
import MMU::*;
import RbmTypes::*;
import MemServerRequest::*;
import MMURequest::*;
import MemServerIndication::*;
import MMUIndication::*;
import MmIndication::*;
import TimerIndication::*;
import TimerRequest::*;

`ifdef MATRIX_TN
import MmRequestTN::*;
import MatrixTN::*;
`else
`ifdef MATRIX_NT
import MmRequestNT::*;
import MatrixNT::*;
`endif
`endif

module  mkConnectalTop#(HostInterface host)(ConnectalTop#(PhysAddrWidth,TMul#(32,N),Empty,NumberOfMasters));

   MmIndicationProxy mmIndicationProxy <- mkMmIndicationProxy(MmIndicationPortal);
   TimerIndicationProxy timerIndicationProxy <- mkTimerIndicationProxy(TimerIndicationPortal);
`ifdef MATRIX_TN
   MmTN#(N) mm <- mkMmTN(mmIndicationProxy.ifc, timerIndicationProxy.ifc, host);
   MmRequestTNWrapper mmRequestWrapper <- mkMmRequestTNWrapper(MmRequestPortal,mm.mmRequest);
`else
`ifdef MATRIX_NT
   MmNT#(N) mm <- mkMmNT(mmIndicationProxy.ifc, timerIndicationProxy.ifc, host);
   MmRequestNTWrapper mmRequestWrapper <- mkMmRequestNTWrapper(MmRequestPortal,mm.mmRequest);
`endif
`endif
   TimerRequestWrapper timerRequestWrapper <- mkTimerRequestWrapper(TimerRequestPortal,mm.timerRequest);
   
   MMUIndicationProxy hostMMUIndicationProxy <- mkMMUIndicationProxy(HostMMUIndication);
   MMU#(PhysAddrWidth) hostMMU <- mkMMU(0, True, hostMMUIndicationProxy.ifc);
   MMURequestWrapper hostMMURequestWrapper <- mkMMURequestWrapper(HostMMURequest, hostMMU.request);

   MemServerIndicationProxy hostMemServerIndicationProxy <- mkMemServerIndicationProxy(HostMemServerIndication);
   MemServer#(PhysAddrWidth, TMul#(32,N), NumberOfMasters) dma <- mkMemServer(mm.readClients, mm.writeClients, cons(hostMMU,nil), hostMemServerIndicationProxy.ifc);
   MemServerRequestWrapper hostMemServerRequestWrapper <- mkMemServerRequestWrapper(HostMemServerRequest, dma.request);

   Vector#(8,StdPortal) portals;
   portals[0] = mmRequestWrapper.portalIfc;
   portals[1] = mmIndicationProxy.portalIfc; 
   portals[2] = hostMemServerRequestWrapper.portalIfc;
   portals[3] = hostMemServerIndicationProxy.portalIfc; 
   portals[4] = timerRequestWrapper.portalIfc;
   portals[5] = timerIndicationProxy.portalIfc; 
   portals[6] = hostMMURequestWrapper.portalIfc;
   portals[7] = hostMMUIndicationProxy.portalIfc;
   let ctrl_mux <- mkSlaveMux(portals);
   
   interface interrupt = getInterruptVector(portals);
   interface slave = ctrl_mux;
   interface masters = dma.masters;
endmodule : mkConnectalTop
