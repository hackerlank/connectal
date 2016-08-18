// Copyright (c) 2016 Connectal Project

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

import Connectable::*;
import FIFOF::*;
import GetPut::*;
import GetPutM::*;

(* always_ready, always_enabled *)
interface AxiStreamMaster#(numeric type dsz);
    method Bit#(dsz)              tdata();
    method Bit#(TDiv#(dsz,8))     tkeep();
    method Bit#(1)                tlast();
    (* prefix = "" *)method Action                 tready((* port="tready" *) Bit#(1) v);
    method Bit#(1)                tvalid();
endinterface

(* always_ready, always_enabled *)
interface AxiStreamSlave#(numeric type dsz);
    (* prefix = "" *)
    method Action      tdata((* port = "tdata" *) Bit#(dsz) v);
    (* prefix = "" *)
    method Action      tkeep((* port = "tkeep" *) Bit#(TDiv#(dsz,8)) v);
    (* prefix = "" *)
    method Action      tlast((* port = "tlast" *) Bit#(1) v);
    method Bit#(1)     tready();
    (* prefix = "" *)
    method Action      tvalid((* port = "tvalid" *)Bit#(1) v);
endinterface

instance Connectable#(AxiStreamMaster#(dataWidth), AxiStreamSlave#(dataWidth));
   module mkConnection#(AxiStreamMaster#(dataWidth) from, AxiStreamSlave#(dataWidth) to)(Empty);
      rule rl_axi_stream;
	 to.tdata(from.tdata());
	 to.tkeep(from.tkeep());
	 to.tlast(from.tlast());
	 to.tvalid(from.tvalid());
	 from.tready(to.tready());
      endrule
   endmodule
endinstance

////////////////////////////////////////////////////////////

instance ToGetM#(AxiStreamMaster#(asz), Bit#(asz));
   module toGetM#(AxiStreamMaster#(asz) m)(Get#(Bit#(asz)));
      FIFOF#(Bit#(asz)) dfifo <- mkFIFOF();

      rule handshake;
         m.tready(pack(dfifo.notFull));
      endrule
      rule enq if (unpack(m.tvalid));
	 dfifo.enq(m.tdata());
      endrule

      return toGet(dfifo);
   endmodule
endinstance

instance ToPutM#(AxiStreamSlave#(asz), Bit#(asz));
   module toPutM#(AxiStreamSlave#(asz) m)(Put#(Bit#(asz)));
      FIFOF#(Bit#(asz)) dfifo <- mkFIFOF();

      rule handshake;
	 m.tvalid(pack(dfifo.notEmpty()));
      endrule
      rule deq if (unpack(m.tready()));
	 m.tdata(dfifo.first());
	 m.tkeep(maxBound);
	 m.tlast(1);
	 dfifo.deq();
      endrule

      return toPut(dfifo);
   endmodule
endinstance

////////////////////////////////////////////////////////////
typeclass ToAxiStream#(type atype, type btype);
   function atype toAxiStream(btype b);
endtypeclass
typeclass MkAxiStream#(type atype, type btype);
   module mkAxiStream#(btype b)(atype);
endtypeclass

instance MkAxiStream#(AxiStreamMaster#(dsize), FIFOF#(Bit#(dsize)));
   module mkAxiStream#(FIFOF#(Bit#(dsize)) f)(AxiStreamMaster#(dsize));
      Wire#(Bool) readyWire <- mkDWire(False);
      rule rl_deq if (readyWire && f.notEmpty);
	 f.deq();
      endrule
     method Bit#(dsize)              tdata();
	if (f.notEmpty())
	  return f.first();
	else
	  return 0;
     endmethod
     method Bit#(TDiv#(dsize,8))     tkeep(); return maxBound; endmethod
     method Bit#(1)                tlast(); return pack(False); endmethod
     method Action                 tready(Bit#(1) v);
	readyWire <= unpack(v);
     endmethod
     method Bit#(1)                tvalid(); return pack(f.notEmpty()); endmethod
   endmodule
endinstance

instance MkAxiStream#(AxiStreamSlave#(dsize), FIFOF#(Bit#(dsize)));
   module mkAxiStream#(FIFOF#(Bit#(dsize)) f)(AxiStreamSlave#(dsize));
      Wire#(Bit#(dsize)) dataWire <- mkDWire(unpack(0));
      Wire#(Bool) validWire <- mkDWire(False);
      rule enq if (validWire && f.notFull());
	 f.enq(dataWire);
      endrule
      method Action      tdata(Bit#(dsize) v);
	 dataWire <= v;
      endmethod
      method Action      tkeep(Bit#(TDiv#(dsize,8)) v); endmethod
      method Action      tlast(Bit#(1) v); endmethod
      method Bit#(1)     tready(); return pack(f.notFull()); endmethod
      method Action      tvalid(Bit#(1) v);
	 validWire <= unpack(v);
      endmethod
   endmodule
endinstance
