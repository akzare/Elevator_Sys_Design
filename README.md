# Elevator algorithm system design project

This repository presents the software system design for the famous elevator algorithm. The reader might consult this link for further information:
https://medium.com/system-designing-interviews/design-a-elevator-system-fc5832ca0b8b

The designed software architecture utilizes the following concepts:
 * Signal/Slot observer design pattern.
 * Multi-threading and thread synchronization
 * Lightweight and efficient network messaging protocol
 * Utilizing C++ as an efficient language for the embedded system and
   python for testing and verification
 

# General architecture

The system design architecture includes two subsystems:
 * Elevator Controller subsystem: It has been designed in C++ and simply simulates the behavior of a typical elevator. It receives the requests throughout a lightweight network messaging protocol over TCP/IP transport layer. The elevator controller acts as the server of this protocol. The incoming traffic from the network is the main controller's process thread over signal/slot observer pattern. Once, the corresponding callback in the controller's process thread receives the event, it pushes it into priority queue to be processed in the earliest time. In parallel, the controller's process thread pops the request elements in the queue and performs the desired actions. During this procedure, it reports the current status of the elevator's car to the requester by the same mechanism (i.e. signal/slot observer pattern and messaging protocol).
 * Requester subsystem: It has been designed in Python and is easily configurable by a JSON file. The JSON configuration layout is defined to include the general system configuration and also it contains the list of test case tasks. Each test case task embodies a state variable which represents its status during its life cycle. 


# Lightweight network messaging protocol

This protocol communicates to other peer by a pre-defined header and payload packet layout. Once each packet is received, it is being acknowledged to the transmitter.


# Contributing

If you'd like to add or improve this software design, your contribution is welcome!


# License

This repository is released under the [MIT license](https://opensource.org/licenses/MIT). In short, this means you are free to use this software in any personal, open-source or commercial projects. Attribution is optional but appreciated.
