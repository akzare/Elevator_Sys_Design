'''
 * @file   ElevatorTestCaseList.py
 * @author Armin Zare Zadeh
 * @date   30 July 2020
 * @version 0.1
 * @brief   Implements a class to hold all the test cases during the program life cycle.
'''
#!/usr/bin/env python3

import sys
import ctypes

import ElevatorConfig as cfg
import ElevatorMsgProtocol as msgProto


class ElevatorTestCaseList:
  '''
  This class builds a test case list out of the configuration
  and holds it during the runtime 
  '''
  def __init__(self, config):
    self.config = config
    self.CallGoTCList = []


  def create_testcase_list(self):
    '''
    Creates a test case list out of the configuration
    '''
      
    # ############################################################
    # Construct 'call' test cases      
    for k in self.config.test_case['call'].keys():
      msgHdr = msgProto.MsgHeader(tx_node_addr = self.config.test_case['call'][k][0], 
                                  rx_node_addr = self.config.test_case['call'][k][1], 
                                  msg_id = self.config.test_case['call'][k][2], 
                                  msg_class = self.config.test_case['call'][k][3],
                                  hdr_len = self.config.network['packet_header_len'],
                                  payload_len = self.config.network['packet_payload_req_len'])
      self.CallGoTCList.append(msgProto.EncodeReqPacket(msg_header = msgHdr, 
                                                        time_tag = self.config.test_case['call'][k][4], 
                                                        req_typ = self.config.usr_request['call'], 
                                                        floor_num = self.config.test_case['call'][k][5], 
                                                        direction = self.config.test_case['call'][k][6],
                                                        go_msg_id = self.config.test_case['call'][k][7],
                                                        state = msgProto.CallGoState.READY2GO))


    # ############################################################
    # Construct 'go' test cases      
    for k in self.config.test_case['go'].keys():
      msgHdr = msgProto.MsgHeader(tx_node_addr = self.config.test_case['go'][k][0], 
                                  rx_node_addr = self.config.test_case['go'][k][1], 
                                  msg_id = self.config.test_case['go'][k][2], 
                                  msg_class = self.config.test_case['go'][k][3],
                                  hdr_len = self.config.network['packet_header_len'],
                                  payload_len = self.config.network['packet_payload_req_len'])
      self.CallGoTCList.append(msgProto.EncodeReqPacket(msg_header = msgHdr, 
                                                        time_tag = self.config.test_case['go'][k][4], 
                                                        req_typ = self.config.usr_request['go'], 
                                                        floor_num = self.config.test_case['go'][k][5],
                                                        direction = 0, 
                                                        go_msg_id = 0, 
                                                        state = msgProto.CallGoState.RESET))
