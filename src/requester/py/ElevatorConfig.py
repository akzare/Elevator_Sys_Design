'''
 * @file   ElevatorConfig.py
 * @author Armin Zare Zadeh
 * @date   29 July 2020
 * @version 0.1
 * @brief   Implements a class to load the configuration form a JSON file.
'''
#!/usr/bin/env python3

import sys
import traceback
import ctypes
import json


class ElevatorConfig:
  '''
  A utility class to load the configuration from a JSON file and
  hold its contents for using during the life of the system 
  '''
  def __init__(self, jsonFileName):
    self.json_fileName = jsonFileName
    # Configuration file information
    self.cfg_file_info = {'name': '', 'date': '', 'author': '', 'rev': '', 'desc': ''}
    
    # General information
    self.general = {'log_file_name': '', 'sim_timestep': ''}
    
    # Network attributes
    self.network = {'type': '', 'addr': '', 'port': '', 'packet_header_len': '', 'packet_payload_req_len': '', 'packet_payload_status_len': ''}

    # Request types: call, go
    self.usr_request = {'call': '', 'go': '', 'status': ''}
    
    # Direction
    self.usr_dir = {'up': '', 'down': ''}

    # Moving Status
    self.move_status = {'moving': '', 'stop': ''}

    # call element: 'id', '(Src_Node_Addr, Dst_Node_Addr, Msg_ID, Msg_Typ, TimeTag, Floor_Num, Direction)'    
    # go element: 'id', '(Src_Node_Addr, Dst_Node_Addr, Msg_ID, Msg_Typ, TimeTag, Floor_Num)'    
    self.test_case = {'call': {}, 'go': {}}


  def load_json(self):
    '''
    Utility member method to open the configuration JSON file and parse it
    '''
    # Opening JSON file 
    with open(self.json_fileName, "r") as cfgFile:  
      # returns JSON object as  
      # a dictionary 
      data = json.load(cfgFile) 
    
      # Iterating through the json 
      # list
      self.cfg_file_info['name'] = data['__cfg_file_info__']['__name__'] 
      self.cfg_file_info['date'] = data['__cfg_file_info__']['__date__'] 
      self.cfg_file_info['author'] = data['__cfg_file_info__']['__author__'] 
      self.cfg_file_info['rev'] = data['__cfg_file_info__']['__rev__'] 
      self.cfg_file_info['desc'] = data['__cfg_file_info__']['__desc__']

      self.general['log_file_name'] = data['__general__']['__log_file_name__']
      self.general['sim_timestep'] = data['__general__']['__sim_timestep__']

      self.network['type'] = data['__network__']['__type__']
      self.network['addr'] = data['__network__']['__addr__'] 
      self.network['port'] = data['__network__']['__port__']
      self.network['packet_header_len'] = data['__network__']['__packet_header_len__']
      self.network['packet_payload_req_len'] = data['__network__']['__packet_payload_req_len__'] 
      self.network['packet_payload_status_len'] = data['__network__']['__packet_payload_status_len__']

      self.usr_request['call'] = data['__usr_request__']['__call__']
      self.usr_request['go'] = data['__usr_request__']['__go__']
      self.usr_request['status'] = data['__usr_request__']['__status__']
      
      self.usr_dir['up'] = data['__usr_dir__']['__up__']
      self.usr_dir['down'] = data['__usr_dir__']['__down__']

      self.move_status['moving'] = data['__move_status__']['__moving__']
      self.move_status['stop'] = data['__move_status__']['__stop__']

      self.test_case['call'] = data['__test_case__']['__call__']
      self.test_case['go'] = data['__test_case__']['__go__']


  def print_config(self):
    '''
    Utility member method to print the configuration
    '''
    print("Configuration:")     
    print("  Configuration File Info:")
    print("    Name: {}".format(self.cfg_file_info['name']))
    print("    Date: {}".format(self.cfg_file_info['date']))
    print("    Author: {}".format(self.cfg_file_info['author']))
    print("    Revision: {}".format(self.cfg_file_info['rev']))
    print("    Description: {}".format(self.cfg_file_info['desc']))

    print("  General:")
    print("    Simulation Time Step: {}".format(self.general['sim_timestep']))
    print("    Log File Name: {}".format(self.general['log_file_name']))

    print("  Network:")
    print("    Type: {}".format(self.network['type']))
    print("    Address: {}".format(self.network['addr']))
    print("    Port: {}".format(self.network['port']))
    print("    Packet Header Length: {}".format(self.network['packet_header_len']))
    print("    Packet Payload Request Length: {}".format(self.network['packet_payload_req_len']))
    print("    Packet Payload Status Length: {}".format(self.network['packet_payload_status_len']))

    print("  Available User Requests:")
    print("    call: {}".format(self.usr_request['call']))
    print("    go: {}".format(self.usr_request['go']))
    print("    status: {}".format(self.usr_request['status']))

    print("  Available Directions:")
    print("    up: {}".format(self.usr_dir['up']))
    print("    down: {}".format(self.usr_dir['down']))

    print("  Available Moving Status:")
    print("    moving: {}".format(self.move_status['moving']))
    print("    stop: {}".format(self.move_status['stop']))

    print("  Test Cases:")
    print("    call: {}".format(self.test_case['call']))
    for k in self.test_case['call'].keys():
      print("      {}:(SrcNode:{})".format(k, self.test_case['call'][k][0]))
      print("      {}:(DstNode:{})".format(k, self.test_case['call'][k][1]))
      print("      {}:(MsgID:{})".format(k, self.test_case['call'][k][2]))
      print("      {}:(MsgTyp:{})".format(k, self.test_case['call'][k][3]))
      print("      {}:(TimeTag:{})".format(k, self.test_case['call'][k][4]))
      print("      {}:(FloorNum:{})".format(k, self.test_case['call'][k][5]))
      print("      {}:(Direction:{})".format(k, self.test_case['call'][k][6]))
      print("      {}:(GoMsgID:{})".format(k, self.test_case['call'][k][7]))
      print("")
    
    print("    go: {}".format(self.test_case['go']))
    for k in self.test_case['go'].keys():
      print("      {}:(SrcNode:{})".format(k, self.test_case['go'][k][0]))
      print("      {}:(DstNode:{})".format(k, self.test_case['go'][k][1]))
      print("      {}:(MsgID:{})".format(k, self.test_case['go'][k][2]))
      print("      {}:(MsgTyp:{})".format(k, self.test_case['go'][k][3]))
      print("      {}:(TimeTag:{})".format(k, self.test_case['go'][k][4]))
      print("      {}:(FloorNum:{})".format(k, self.test_case['go'][k][5]))
      print("")
