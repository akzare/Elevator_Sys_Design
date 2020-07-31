'''
 * @file   ElevatorMsgProtocol.py
 * @author Armin Zare Zadeh
 * @date   24 July 2020
 * @version 0.1
 * @brief   A simple messaging protocol implementation.
'''

#!/usr/bin/env python3


import struct
from enum import Enum, auto



########################################################################
# C A L L   G O   S T A T E   E N U M   C L A S S
########################################################################
class CallGoState(Enum):
  '''
  This enumeration class defines the states for the life cycle of each
  test case item.
  '''
  RESET = auto()
  READY2GO = auto()
  IN_PROGRESS = auto()
  ACKED = auto()
  NACKED = auto()
  REACHED = auto()
  FINISHED = auto()


####################################################################
# https://gist.github.com/oysstu/68072c44c02879a2abf94ef350d1c7c6
####################################################################
def crc16(data: bytes, poly=0x8408):
  '''
  CRC-16-CCITT Algorithm
  '''
  # Debugging: printing the incoming byte array
#  gen = ":".join("{:02x}".format(d) for d in data)
#  print({gen})
    
  crc = 0xFFFF
  for b in data:
    cur_byte = 0xFF & b
    for _ in range(0, 8):
      if (crc & 0x0001) ^ (cur_byte & 0x0001):
        crc = (crc >> 1) ^ poly
      else:
        crc >>= 1
      cur_byte >>= 1
  crc = (~crc & 0xFFFF)
  crc = (crc << 8) | ((crc >> 8) & 0xFF)
  
  return crc & 0xFFFF


########################################################################
# M E S S A G E   H E A D E R   C L A S S
########################################################################
class MsgHeader:
  '''
  This class constructs the Message header and serializes it into raw  
  buffer for transmission over the network
  '''
  def __init__(self, tx_node_addr, rx_node_addr, msg_id, msg_class, hdr_len, payload_len):
    self.magic = 0x0E
    self.tx_node_addr = tx_node_addr
    self.rx_node_addr = rx_node_addr
    self.msg_class = msg_class
    self.msg_id = msg_id
    self.hdr_len = hdr_len
    self.msg_len = payload_len + hdr_len
    self.raw = 0
    self.data = 0
    self.getData()
    self.serialize()


  def getData(self):
    '''
    Get a data buffer containing the entire header packet in bytes
    '''
    # HEADER
    #   magic: 1-byte
    #   tx_node_addr: 2-bytes
    #   rx_node_addr: 2-bytes
    #   msg_class: 1-byte
    #   msg_id: 2-byte
    #   len: 2-byte
    self.data = [self.magic & 0xFF] + \
                [(self.tx_node_addr>>(8*i))&0xFF for i in range(1,-1,-1)] + \
                [(self.rx_node_addr>>(8*i))&0xFF for i in range(1,-1,-1)] + \
                [self.msg_class & 0xFF] + \
                [(self.msg_id>>(8*i))&0xFF for i in range(1,-1,-1)] + \
                [(self.msg_len>>(8*i))&0xFF for i in range(1,-1,-1)]


  def serialize(self):
    '''
    Serializes the header packet into a raw buffer
    '''
    # ############################################################
    # Cheat-sheet for using different data formats with Python struct
    # f -> C Type:float, Python Type: float, Standard size: 4
    # d -> C Type:double, Python Type: float, Standard size: 8
    # Q -> C Type:unsigned long long, Python Type: integer, Standard size: 8
    # L -> C Type:unsigned long, Python Type: integer, Standard size: 4
    # B -> C Type:unsigned char, Python Type: integer, Standard size: 1
    # H -> C Type:unsigned short, Python Type: integer, Standard size: 2
    # I -> C Type:unsigned int, Python Type: integer, Standard size: 4
    # x -> C Type:pad byte, Python Type: no value, Standard size: 

    # ############################################################
    # HEADER LAYOUT IN MEMORY
    #   magic: 1-byte
    #   tx_node_addr: 2-bytes
    #   rx_node_addr: 2-bytes
    #   msg_class: 1-byte
    #   msg_id: 2-byte
    #   len: 2-byte
    
    self.raw = struct.pack('!BHHBHH',
                            self.magic,
                            self.tx_node_addr,
                            self.rx_node_addr,
                            self.msg_class,
                            self.msg_id,
                            self.msg_len)



########################################################################
# E N C O D E R   R E Q U E S T E R   P A C K E T   C L A S S
########################################################################
class EncodeReqPacket:
  '''
  This class constructs the entire Request Packet for sending over
  network to the elevator's controller subsystem
  '''
  def __init__(self, msg_header, time_tag, req_typ, floor_num, direction, go_msg_id, state):
    self.msg_header = msg_header
    self.time_tag = time_tag
    self.req_typ = req_typ
    self.floor_num = floor_num
    self.direction = direction
    self.go_msg_id = go_msg_id
    self.state = state
    self.is_ok = 0
    self.crc16 = 0
    self.raw = 0
    self.data = 0
    self.calcCRC()


  def calcCRC(self):
    '''
    Helper method to calculate the packet's CRC
    '''
    # CHECKSUM
    #   crc: 2-byte
    self.getData()
    self.crc16 = crc16(self.data)


  def getData(self):
    '''
    Get a data buffer containing the entire packet (header + payload) in bytes
    '''
    # PAYLOAD
    #   request type: 1-byte
    #   floor number: 1-byte
    #   direction: 1-byte
    self.data = self.msg_header.data + \
                [(self.time_tag>>(8*i))&0xFF for i in range(7,-1,-1)] + \
                [self.req_typ & 0xFF] + \
                [self.floor_num & 0xFF] + \
                [self.direction & 0xFF]


  def serialize(self):
    '''
    Serializes the whole packet (header + payload) into a raw buffer
    '''

    # ############################################################
    # Cheat-sheet for using different data formats with Python struct
    # f -> C Type:float, Python Type: float, Standard size: 4
    # d -> C Type:double, Python Type: float, Standard size: 8
    # Q -> C Type:unsigned long long, Python Type: integer, Standard size: 8
    # L -> C Type:unsigned long, Python Type: integer, Standard size: 4
    # B -> C Type:unsigned char, Python Type: integer, Standard size: 1
    # H -> C Type:unsigned short, Python Type: integer, Standard size: 2
    # I -> C Type:unsigned int, Python Type: integer, Standard size: 4
    # x -> C Type:pad byte, Python Type: no value, Standard size: 

    self.raw = self.msg_header.raw
    
    # ############################################################
    # PAYLOAD LAYOUT IN MEMORY
    #   timetag: 8-byte
    #   request type: 1-byte
    #   floor number: 1-byte
    #   direction: 1-byte
    # CHECKSUM
    #   crc: 2-byte
    
    self.raw = self.raw + struct.pack('!QBBBH',
                            self.time_tag,
                            self.req_typ,
                            self.floor_num,
                            self.direction,
                            self.crc16)




########################################################################
# D E C O D E R   R E C E I V E R   P A C K E T   C L A S S
########################################################################
class DecodeRecPacket:
  '''
  This class reconstructs the entire Received Packet from the 
  network and checks its correctness
  '''
  def __init__(self, binBuf, hdr_len, payload_len):
    self.binBuf = binBuf
    self.msg_header = MsgHeader(tx_node_addr = 0, rx_node_addr = 0, msg_id = 0, msg_class = 0, hdr_len = 0, payload_len = 0)
    self.exp_hdr_len = hdr_len
    self.exp_payload_len = payload_len
    self.len = 0
    self.is_ok = 1   
    self.is_ctrl_ack = 0
    self.is_ctrl_nck = 0
    self.is_data = 0
    self.time_tag = 0
    self.req_typ = 0
    self.move_status = 0
    self.floor_num = 0
    self.crc16 = 0
    self.exp_crc16 = 0
    self.deserialize()


  def deserialize(self):
    '''
    De-Serializes the whole packet (header + payload) into header and payload data structures
    '''

    # ############################################################
    # Cheat-sheet for using different data formats with Python struct
    # f -> C Type:float, Python Type: float, Standard size: 4
    # d -> C Type:double, Python Type: float, Standard size: 8
    # Q -> C Type:unsigned long long, Python Type: integer, Standard size: 8
    # L -> C Type:unsigned long, Python Type: integer, Standard size: 4
    # B -> C Type:unsigned char, Python Type: integer, Standard size: 1
    # H -> C Type:unsigned short, Python Type: integer, Standard size: 2
    # I -> C Type:unsigned int, Python Type: integer, Standard size: 4
    # x -> C Type:pad byte, Python Type: no value, Standard size: 

    exp_len = 0
    self.msg_header.magic = struct.unpack('!B', self.binBuf[0:1])[0]
    if self.msg_header.magic != 0x0E:
      self.is_ok = 0
      print("DecodeRecPacket: deserialize: Corrupted message: Wrong Magic Word: {}".format(self.msg_header.magic))
    self.msg_header.tx_node_addr = struct.unpack('!H', self.binBuf[1:3])[0]
    self.msg_header.rx_node_addr = struct.unpack('!H', self.binBuf[3:5])[0]
    self.msg_header.msg_class = struct.unpack('!B', self.binBuf[5:6])[0]
    if (self.msg_header.msg_class & 0xC1) == 0xC1:
      self.is_ctrl_ack = 1
      exp_len = self.exp_hdr_len
    elif (self.msg_header.msg_class & 0x81) == 0x81:
      self.is_ctrl_nck = 1
      exp_len = self.exp_hdr_len
    elif (self.msg_header.msg_class & 0x02) == 0x02:
      self.is_data = 1
      exp_len = self.exp_hdr_len + self.exp_payload_len
    else:
      self.is_ok = 0
      print("DecodeRecPacket: deserialize: Corrupted message: Wrong Message Class: {:02x}".format(self.msg_header.msg_class))
    self.msg_header.msg_id = struct.unpack('!H', self.binBuf[6:8])[0]
    self.msg_header.msg_len = struct.unpack('!H', self.binBuf[8:10])[0]

    # Check Length
    if self.msg_header.msg_len != exp_len:
      self.is_ok = 0
      print("DecodeRecPacket: deserialize: Corrupted message: Wrong Message Length: {:04x}, expected:{:04x}".format(self.msg_header.msg_len, exp_len))
    
    if self.is_data == 1:
      self.time_tag = struct.unpack('!Q', self.binBuf[10:18])[0]
      self.req_typ = struct.unpack('!B', self.binBuf[18:19])[0]
      self.floor_num = struct.unpack('!B', self.binBuf[19:20])[0]
      self.move_status = struct.unpack('!B', self.binBuf[20:21])[0]
      self.crc16 = struct.unpack('!H', self.binBuf[21:23])[0]
      
      self.calcCRC()
      if self.exp_crc16 != self.crc16:
        self.is_ok = 0
        print("DecodeRecPacket: deserialize: Corrupted message: Wrong Message CRC16: {:04x}, expected:{:04x}".format(self.crc16, self.exp_crc16))


  def calcCRC(self):
    '''
    Helper method to calculate the packet's CRC
    '''
    # CHECKSUM
    #   crc: 2-byte
    self.getData()
    self.exp_crc16 = crc16(self.data)


  def getData(self):
    '''
    Get a data buffer containing the entire packet (header + payload) in bytes
    '''
    self.msg_header.getData()
    # PAYLOAD
    #   request type: 1-byte
    #   floor number: 1-byte
    #   direction: 1-byte
    self.data = self.msg_header.data + \
                [(self.time_tag>>(8*i))&0xFF for i in range(7,-1,-1)] + \
                [self.req_typ & 0xFF] + \
                [self.floor_num & 0xFF] + \
                [self.move_status & 0xFF]
#   Debugging
#    gen = ":".join("{:02x}".format(d) for d in self.data)
#    print({gen})

