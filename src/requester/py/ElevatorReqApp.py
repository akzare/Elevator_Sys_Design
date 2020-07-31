'''
 * @file   ElevatorReqApp.py
 * @author Armin Zare Zadeh
 * @date   24 July 2020
 * @version 0.1
 * @brief   This Python module acts as a simulator for the elevator's 
 *          C++ controller module. It connects to the controller over
 *          TCP/IP transport and sends the sequence of requests loaded 
 *          from a JSON file to the controller. 
'''

#!/usr/bin/env python3

import sys
import socket
import selectors
import traceback
import ctypes
import json
from time import sleep

import ElevatorNetProtocol as netProtocol
import ElevatorMsgProtocol as msgProtocol
import ElevatorConfig as cfg
import ElevatorTestCaseList as tcList


# ToDo: The location of the JSON configuration file is fixed here. It should be determined by user
json_fileName = ('C:\\elevator\\src\\config\\elevator_cfg.json')          


# ToDo: The main application test case task list scheduler does not consider the time step. It should
#       be further modified to schedule the sequence of items based on the corresponding time steps. 

########################################################################
# R E Q U E S T E R   M A I N   A P P   C L A S S
########################################################################
class ElevatorReqApp:
  '''
  This is the main application class which runs the whole system.
  '''
  def __init__(self):
    # Load configuration
    self.config = cfg.ElevatorConfig(json_fileName)
    self.config.load_json()
    self.config.print_config()
    
    # Create Test Case List out of configuration
    self.testcase = tcList.ElevatorTestCaseList(self.config)
    self.testcase.create_testcase_list()
    
    # async select for handling socket
    self.sel = selectors.DefaultSelector()
    
    # A counter to check whether all test cases are finished
    self.allDoneCntr = 0


  def startConn(self):
    '''
    This member method establishes the connection to the elevator C++ module.
    '''
    addr = (self.config.network['addr'], self.config.network['port'])
    print("starting connection to", addr)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setblocking(False)
    sock.connect_ex(addr)
    events = selectors.EVENT_READ | selectors.EVENT_WRITE
    netProto = netProtocol.NetProtocol(self.sel, sock, addr, self.config, self.testcase)
    self.sel.register(sock, events, data=netProto)
    return sock


  def scheduleElevatorReqList(self):
    '''
    Schedule the request list
    '''
    curReqList = 0
    
    # ############################################################
    # Traversing the test case list and schedule the execution of 
    # each item which is in READY2GO state.
    # Each item in test case list should go through this sequence
    # of states:
    #
    #   'call' items:
    #       READY2GO -> IN_PROGRESS -> ACKED/NACKED -> REACHED -> FINISHED
    #   'go' items:
    #       RESET -> READY2GO -> IN_PROGRESS -> ACKED/NACKED -> REACHED -> FINISHED
    #
    # Basically, each 'call' item is linked to its corresponding 'go' item. Hence,
    # each 'go' items waits until its corresponding 'call' item reaches the
    # 'REACHED' state.
    #
    # States are defined based on the following concept: 
    #   RESET        : Test case task item stays idling in this state.
    #   READY2GO     : It is read to go and is being executed by scheduler. The
    #                  scheduler sends a request to the elevator's controller.
    #   IN_PROGRESS  : System is waiting for the elevator's controller to send back
    #                  the ACK/NACK to this request.
    #   ACKED/NACKED : The ACK/NACK packet is received.
    #   REACHED      : We received a status packet for the elevator's controller to
    #                  let us know the status of car's movement.
    #   FINISHED     : The elevator's car is already reached the requested destination
    #                  and our desired request is successfully achieved.
    for i in range(0, len(self.testcase.CallGoTCList)):
      if self.testcase.CallGoTCList[i].state == msgProtocol.CallGoState.RESET:
        pass
      elif self.testcase.CallGoTCList[i].state == msgProtocol.CallGoState.READY2GO:
        print("scheduleElevatorReqList: New Task: tx_node_addr={:04x}, rx_node_addr=0x{:04x}, msg_class={:02x}, msg_id={:04x}, msg_len={:04x}, time_tag={:08x}, req_typ={:02x}, floor_num={:02x}, dir={:02x}, crc={:04x}".format(self.testcase.CallGoTCList[i].msg_header.tx_node_addr, self.testcase.CallGoTCList[i].msg_header.rx_node_addr, self.testcase.CallGoTCList[i].msg_header.msg_class, self.testcase.CallGoTCList[i].msg_header.msg_id, self.testcase.CallGoTCList[i].msg_header.msg_len, self.testcase.CallGoTCList[i].time_tag, self.testcase.CallGoTCList[i].req_typ, self.testcase.CallGoTCList[i].floor_num, self.testcase.CallGoTCList[i].direction, self.testcase.CallGoTCList[i].crc16))
        
        self.testcase.CallGoTCList[i].state = msgProtocol.CallGoState.IN_PROGRESS
        curReqList = self.testcase.CallGoTCList[i]

      elif self.testcase.CallGoTCList[i].state == msgProtocol.CallGoState.IN_PROGRESS:
        pass
      elif self.testcase.CallGoTCList[i].state == msgProtocol.CallGoState.ACKED:
        print("scheduleElevatorReqList: Task acknowledged: msg_id={:04x}, req_typ={:02x}, floor_num={:02x}".format(self.testcase.CallGoTCList[i].msg_header.msg_id, self.testcase.CallGoTCList[i].req_typ, self.testcase.CallGoTCList[i].floor_num))
      elif self.testcase.CallGoTCList[i].state == msgProtocol.CallGoState.NACKED:
        print("scheduleElevatorReqList: Task nacknowledged: msg_id={:04x}, req_typ={:02x}, floor_num={:02x}".format(self.testcase.CallGoTCList[i].msg_header.msg_id, self.testcase.CallGoTCList[i].req_typ, self.testcase.CallGoTCList[i].floor_num))
        raise Exception('scheduleElevatorReqList: Task Nack')
      elif self.testcase.CallGoTCList[i].state == msgProtocol.CallGoState.REACHED:
        print("scheduleElevatorReqList: Task reached: msg_id={:04x}, req_typ={:02x}, floor_num={:02x}".format(self.testcase.CallGoTCList[i].msg_header.msg_id, self.testcase.CallGoTCList[i].req_typ, self.testcase.CallGoTCList[i].floor_num))

        for j in range(0, len(self.testcase.CallGoTCList)):
          if self.testcase.CallGoTCList[i].go_msg_id == self.testcase.CallGoTCList[j].msg_header.msg_id and self.testcase.CallGoTCList[j].req_typ == self.config.usr_request['go']:
            self.testcase.CallGoTCList[j].state = msgProtocol.CallGoState.READY2GO
        self.testcase.CallGoTCList[i].state = msgProtocol.CallGoState.FINISHED
        self.allDoneCntr = self.allDoneCntr + 1
      elif self.testcase.CallGoTCList[i].state == msgProtocol.CallGoState.FINISHED:
        print("scheduleElevatorReqList: Task finished: msg_id={:04x}, req_typ={:02x}, floor_num={:02x}".format(self.testcase.CallGoTCList[i].msg_header.msg_id, self.testcase.CallGoTCList[i].req_typ, self.testcase.CallGoTCList[i].floor_num))
      else:
        print("scheduleElevatorReqList: Task unknown state: msg_id={:04x}, req_typ={:02x}, floor_num={:02x}".format(self.testcase.CallGoTCList[i].msg_header.msg_id, self.testcase.CallGoTCList[i].req_typ, self.testcase.CallGoTCList[i].floor_num))

    return curReqList


  def run(self):
    '''
    This is the main loop of the simulator requester application
    '''
    sockFileObj = self.startConn()

    try:
      while True:
        curReqList = self.scheduleElevatorReqList()

        if curReqList != 0:
          key = self.sel.get_key(sockFileObj)
          netProto = key.data
          netProto._set_selector_events_mask("rw")

        events = self.sel.select(timeout=1)
        for key, mask in events:
          netProto = key.data
          try:
            netProto.process_events(mask, curReqList)
            curReqList = []

          except Exception:
            print(
                "main: error: exception for",
                f"{netProto.addr}:\n{traceback.format_exc()}",
            )
            netProto.close()
        # Check for a socket being monitored to continue.
        if not self.sel.get_map():
          break

    #    if len(self.testcase.CallGoTCList) == 0:
        if self.allDoneCntr == len(self.testcase.CallGoTCList):        
          print('All tasks are done! Closing the connection and quit...')

          netProto.close()
          break

    except KeyboardInterrupt:
      print("caught keyboard interrupt, exiting")
    finally:

      print('Closing select...')
      self.sel.close()



########################################################################
# M A I N   P R O C E D U R E
########################################################################
if __name__ == '__main__':
  # Instantiate a new elevator's requester application and run it
  clientApi = ElevatorReqApp()
  clientApi.run()
