'''
 * @file   ElevatorNetProtocol.py
 * @author Armin Zare Zadeh
 * @date   24 July 2020
 * @version 0.1
 * @brief   This file implements a simple net protocol for communication
 *          with the elevator's controller. 
'''

import sys
import selectors
import json
import io
import struct

import ElevatorMsgProtocol as msgProtocol


########################################################################
# P R I N T   A R R A Y   O F   D A T A
########################################################################
def print_array(dataArray):
  gen = ":".join("{:02x}".format(d) for d in dataArray)
  print({gen})
  
  
########################################################################
# N E T W O R K   P R O T O C O L   C L A S S
########################################################################
class NetProtocol:
  '''
  This class implements a network protocol to communicate with 
  the elevator's controller subsystem
  '''
  def __init__(self, selector, sock, addr, config, testcase):
    self.selector = selector
    self.sock = sock
    self.addr = addr
    self.config = config
    self.testcase = testcase
    self._recv_buffer = b""
    self._send_buffer = b""
    self.response = None


  def _set_selector_events_mask(self, mode):
    """Set selector to listen for events: mode is 'r', 'w', or 'rw'."""
    if mode == "r":
      events = selectors.EVENT_READ
    elif mode == "w":
      events = selectors.EVENT_WRITE
    elif mode == "rw":
      events = selectors.EVENT_READ | selectors.EVENT_WRITE
    else:
      raise ValueError(f"Invalid events mask mode {repr(mode)}.")
    self.selector.modify(self.sock, events, data=self)


  def _read(self):
    '''
    Receive a raw buffer from a source node
    '''
    try:       
      # Should be ready to read
      data = self.sock.recv(4096)
      
    except BlockingIOError:
      # Resource temporarily unavailable (errno EWOULDBLOCK)
      pass
    else:
      if data:
        self._recv_buffer = data
      else:
        raise RuntimeError("Peer closed.")


  def _write(self):
    '''
    Xmit a raw buffer to destination node
    '''
    if self._send_buffer:
      print("sending", repr(self._send_buffer), "to", self.addr)
      try:
        # Should be ready to write
        sent = self.sock.send(self._send_buffer)
      except BlockingIOError:
        # Resource temporarily unavailable (errno EWOULDBLOCK)
        pass
      else:
        self._send_buffer = self._send_buffer[sent:]


  def _create_message(self, curTCList):
    '''
    Constructs a new packet by using the message protocol
    '''
    curTCList.serialize()
    return curTCList.raw


  def _process_response_binary_content(self):
    '''
    Utility method to process the received binary buffer
    '''
    content = self.response
    print(f"got response: {repr(content)}")
   
    # Decoding the received packet 
    recMsg = msgProtocol.DecodeRecPacket(content, self.config.network['packet_header_len'], self.config.network['packet_payload_status_len'])

    # Debugging the result of debugging
    if recMsg.is_ok == 1:
      print("Received Packet")
      print("  Header : tx_node_addr={:04x}, rx_node_addr=0x{:04x}, msg_class={:02x}, msg_id={}".format(recMsg.msg_header.tx_node_addr, recMsg.msg_header.rx_node_addr, recMsg.msg_header.msg_class, recMsg.msg_header.msg_id))
      if recMsg.is_ctrl_ack == 1:
        print("  Control Message: ACK")
      elif recMsg.is_ctrl_nck == 1:
        print("  Control Message: NACK")
      elif recMsg.is_data == 1:
        if self.config.usr_request['status'] != recMsg.req_typ:          
          print("  Data Message: Unknown request type: {}".format(recMsg.req_typ))    
        else:  
          print("  Data Message:")    
          print("    timetag={:08x}, req_typ={}, move_status={}, floor_num={}".format(recMsg.time_tag, recMsg.req_typ, recMsg.move_status, recMsg.floor_num))
    
      # ############################################################
      # Traversing the test case list and update the current status 
      # the corresponding item
      for j in range(0, len(self.testcase.CallGoTCList)):
        if self.testcase.CallGoTCList[j].msg_header.msg_id == recMsg.msg_header.msg_id:
          if recMsg.is_ctrl_ack == 1:
            self.testcase.CallGoTCList[j].state = msgProtocol.CallGoState.ACKED
            self.testcase.CallGoTCList[j].is_ok = 1
          elif recMsg.is_ctrl_nck == 1:
            self.testcase.CallGoTCList[j].state = msgProtocol.CallGoState.NACKED
            self.testcase.CallGoTCList[j].is_ok = 0
          elif recMsg.is_data == 1:
            if self.config.usr_request['status'] == recMsg.req_typ:          
              if self.testcase.CallGoTCList[j].floor_num == recMsg.floor_num and recMsg.move_status == self.config.move_status['stop']:
                self.testcase.CallGoTCList[j].state = msgProtocol.CallGoState.REACHED
                
          break      


  def process_events(self, mask, curTCList):
    '''
    Processing the incoming events from select
    '''
    if mask & selectors.EVENT_READ:
      self.read()
    if mask & selectors.EVENT_WRITE:
      self.write(curTCList)


  def read(self):
    '''
    read method which is invoked by process event
    '''
    self._read()

    if self.response is None:
      self.process_response()

    # Set selector to listen for read events, or writing.
    self._set_selector_events_mask("r")
      

  def write(self, curTCList):
    '''
    write method which is invoked by process event
    '''
    if curTCList != 0:
      self._send_buffer = self._create_message(curTCList)
      self._write()

    # Set selector to listen for read events, or writing.
    self._set_selector_events_mask("r")



  def close(self):
    '''
    close method which is invoked on closing the connection
    '''
    print("closing connection to", self.addr)
    try:
      self.selector.unregister(self.sock)
    except Exception as e:
      print(
          f"error: selector.unregister() exception for",
          f"{self.addr}: {repr(e)}",
      )

    try:
      self.sock.close()
    except OSError as e:
      print(
          f"error: socket.close() exception for",
          f"{self.addr}: {repr(e)}",
      )
    finally:
      # Delete reference to socket object for garbage collection
      self.sock = None


  def process_response(self):
    '''
    Processing the response
    '''
    # Binary or unknown content-type
    self.response = self._recv_buffer
    print(
        f'received response from',
        self.addr,
    )
    self._process_response_binary_content()
    self.response = None
    # Close when response has been processed
#    self.close()
