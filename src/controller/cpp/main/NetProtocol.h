/*
 * @file   NetProtocol.h
 * @author Armin Zare Zadeh, ali.a.zarezadeh@gmail.com
 * @date   25 July 2020
 * @version 0.1
 * @brief   A simple implementation of message queuing protocol
 *          stack for easy and lightweight communication between
 *          multiple nodes over Socket transport.
 *          It is being used for delivering the elevator's user
 *          requests to the main controller.
 */

#ifndef D_NET_PROTOCOL_H
#define D_NET_PROTOCOL_H

#include "StoppableTask.h"
#include "signal_slot.h"
#include "NonCopyable.h"
#include "Request.h"
#include "TransportSocket.h"

#include <Winsock.h>

#include <iostream>

#include <unistd.h>

#include <algorithm>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>

#include <functional>
#include <memory>
#include <mutex>
#include <vector>


namespace Net {

static const uint16_t NODE_ADDRESS = 0x3E8;


// CRC16-CCITT Implementation
static const uint16_t POLY = 0x8408;
uint16_t crc16(const std::vector<uint8_t>& data) {
  uint16_t crc = 0xFFFF;
  std::cout << std::endl;
  for (auto b : data) {
    std::cout << (b&0xff) << ":";
    uint8_t cur_byte = 0xFF & b;
    for (int i=0; i<8; i++) {
      if ((crc & 0x0001) ^ (cur_byte & 0x0001))
        crc = (crc >> 1) ^ POLY;
      else
        crc >>= 1;
      cur_byte >>= 1;
    }
  }
  std::cout << std::endl;
  crc = (~crc & 0xFFFF);
  crc = (crc << 8) | ((crc >> 8) & 0xFF);

  return crc & 0xFFFF;
}


// https://stackoverflow.com/questions/809902/64-bit-ntohl-in-c
uint64_t ntoh64(const uint64_t *input) {
  uint64_t rval;
  uint8_t *data = (uint8_t *)&rval;

  data[0] = *input >> 56;
  data[1] = *input >> 48;
  data[2] = *input >> 40;
  data[3] = *input >> 32;
  data[4] = *input >> 24;
  data[5] = *input >> 16;
  data[6] = *input >> 8;
  data[7] = *input >> 0;

  return rval;
}

#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))


// This class implements a very compact and lightweight messaging protocol on top of
// socket transport layer. It includes the link
class MsgProtocol : noncopyable {
public:
  //---------------------------------------------------------------------------
  //
  // Definitions for Protocol Messages
  //
  //---------------------------------------------------------------------------

  // node_addr_t is end point's address
  using node_addr_t = uint16_t;


  // msg_id_t is a unique message that an end point can transmit or listen for.
  using msg_id_t = uint16_t;


  // msg_magic is first byte in stream used to help identify our MsgProtocol frames
  using msg_magic_t = uint8_t;


  // msg_class is the type of message an end point is sending plus
  // optional ACK or NAK bits set.  Thus msg_class composes two fields.
  // Bits 7-6 = ACK/NAK Field
  // Bits 5-0 = msg_type
  using msg_class_t = uint8_t;
  using msg_type_t = uint8_t;


  // msg_len is the length of the message + the trailing CRC
  using msg_len_t = uint16_t;


  // CRC
  // CRC used to detect errors.  The CRC covers both the message header and payload.
  using msg_crc_t = uint16_t;


  // MagicValue is how we identify messages that are our MsgProtocol messages.
  static const msg_magic_t MagicValue = 0x0E;

  // Bit: 7 6 5 4 3 2 1 0 Desc
  //    : 0 0 x x x x x x Msg
  //    : 0 1 x x x x x x undefined/reserved
  //    : 1 0 x x x x x x Msg_NAK
  //    : 1 1 x x x x x x Msg_ACK
  // -----------------------------

  #define OPTYPE_BITMASK_SIZE  2  // size in bits
  #define TMSG_OPTYPE_MASK(x) x << ((sizeof(msg_type_t)*8)-OPTYPE_BITMASK_SIZE)

  enum class MSG_OPTYPE : uint8_t
  {
    OP_MSG  = TMSG_OPTYPE_MASK(0x00),  // 00xxxxxx
    OP_NAK  = TMSG_OPTYPE_MASK(0x02),  // 10xxxxxx
    OP_ACK  = TMSG_OPTYPE_MASK(0x03),  // 11xxxxxx
    OP_MASK = TMSG_OPTYPE_MASK(0x03)   // 11xxxxxx
  };



  enum class MSGTYPE : uint8_t
  {
    MSG_CTRL = 1,
    MSG_DATA = 2,
    MSG_UNKNOWN
  };


  // structure of a Message header
  // full message looks like: [HEADER][payload....][crc]
  #pragma pack(push, 1)
  struct msg_hdr_t {
    msg_magic_t  magic;        // 1-byte
    node_addr_t  tx_node_addr; // 2-byte
    node_addr_t  rx_node_addr; // 2-byte
    msg_class_t  msg_class;    // 1-byte
    msg_id_t     msg_id;       // 2-byte
    msg_len_t    len;          // 2-byte
  };
  #pragma pack(pop)


  using req_time_t = uint64_t;
  using req_cmd_t = uint8_t;
  using req_floor_t = uint8_t;
  using req_dir_t = uint8_t;

  // structure of payload
  #pragma pack(push, 1)
  struct msg_payload_t {
    req_time_t  timetag;   // 8-byte
    req_cmd_t   command;   // 1-byte
    req_floor_t floor_num; // 1-byte
    req_dir_t   direction; // 1-byte
  };
  #pragma pack(pop)



  // https://stackoverflow.com/questions/16519846/parse-ip-and-tcp-header-especially-common-tcp-header-optionsof-packets-capture
  // Helper static function to de-serialize the packet's header
  static msg_hdr_t deserialize_header(std::istream& stream, bool ntoh) {
    msg_hdr_t header;

    stream.read((char*)&header.magic, sizeof(header.magic));
    stream.read((char*)&header.tx_node_addr, sizeof(header.tx_node_addr));
    stream.read((char*)&header.rx_node_addr, sizeof(header.rx_node_addr));
    stream.read((char*)&header.msg_class, sizeof(header.msg_class));
    stream.read((char*)&header.msg_id, sizeof(header.msg_id));
    stream.read((char*)&header.len, sizeof(header.len));
    if(ntoh) {
      header.tx_node_addr = ntohs(header.tx_node_addr);
      header.rx_node_addr = ntohs(header.rx_node_addr);
      header.msg_id =       ntohs(header.msg_id);
      header.len =          ntohs(header.len);
    }
    return header;
  }


  // Helper static function to de-serialize the packet's payload
  static msg_payload_t deserialize_payload(std::istream& stream, bool ntoh) {
    msg_payload_t payload;

    stream.read((char*)&payload.timetag, sizeof(payload.timetag));
    stream.read((char*)&payload.command, sizeof(payload.command));
    stream.read((char*)&payload.floor_num, sizeof(payload.floor_num));
    stream.read((char*)&payload.direction, sizeof(payload.direction));
    if(ntoh) {
      payload.timetag = ntoh64(&payload.timetag);
    }
    return payload;
  }


  // Helper static function to serialize the packet's header
  static void serialize_header(std::ostream& stream, msg_hdr_t& header, bool hton) {
    if(hton) {
      header.tx_node_addr = htons(header.tx_node_addr);
      header.rx_node_addr = htons(header.rx_node_addr);
      header.msg_id =       htons(header.msg_id);
      header.len =          htons(header.len);
    }
    stream.write((char*)&header.magic, sizeof(header.magic));
    stream.write((char*)&header.tx_node_addr, sizeof(header.tx_node_addr));
    stream.write((char*)&header.rx_node_addr, sizeof(header.rx_node_addr));
    stream.write((char*)&header.msg_class, sizeof(header.msg_class));
    stream.write((char*)&header.msg_id, sizeof(header.msg_id));
    stream.write((char*)&header.len, sizeof(header.len));
  }


  // Helper static function to serialize the packet's payload
  static void serialize_payload(std::ostream& stream, msg_payload_t& payload, bool hton) {
    if(hton) {
      payload.timetag = htonll(payload.timetag);
    }
    stream.write((char*)&payload.timetag, sizeof(payload.timetag));
    stream.write((char*)&payload.command, sizeof(payload.command));
    stream.write((char*)&payload.floor_num, sizeof(payload.floor_num));
    stream.write((char*)&payload.direction, sizeof(payload.direction));
  }


  // Helper static function to serialize the packet's payload
  static void serialize_crc16(std::ostream& stream, uint16_t crc, bool hton) {
    if(hton) {
      crc = htons(crc);
    }
    stream.write((char*)&crc, sizeof(crc));
  }


  // Helper static function to check the packet's health
  static bool header_check(const msg_hdr_t& header, msg_len_t exp_len) {
    bool result = true;
    if (header.magic != MagicValue) {
      result = false;
      std::cout << "Got corrupted packet: Header magic value is wrong: " << header.magic << " Expected:" << MagicValue << std::endl;
    }

    if (header.len != exp_len) {
      result = false;
      std::cout << "Got corrupted packet: Header length value is wrong: " << header.len << " Expected:" << exp_len << std::endl;
    }

    if (header.msg_class != static_cast<msg_class_t>(MSGTYPE::MSG_DATA)) {
      result = false;
      std::cout << "Got corrupted packet: Header message class is wrong: " << header.msg_class << " Expected:" << static_cast<int>(MSGTYPE::MSG_DATA) << std::endl;
    }

    return result;
  }


  // Helper static function to print the packet's header
  static void print_header(const msg_hdr_t& header) {
    std::cout << "msg header(size:" << sizeof(msg_hdr_t)<< "):" << std::endl;
    std::cout << "  magic:" << std::hex << (header.magic & 0xFF) << std::endl;
    std::cout << "  tx node addr:" << std::hex << (header.tx_node_addr & 0xFFFF) << std::endl;
    std::cout << "  rx node addr:" << std::hex << (header.rx_node_addr & 0xFFFF) << std::endl;
    std::cout << "  msg class:" << std::hex << (header.msg_class & 0xFF) << std::endl;
    std::cout << "  msg id:" << std::hex << (header.msg_id & 0xFFFF) << std::endl;
    std::cout << "  len:" << std::hex << (header.len & 0xFFFF) << std::endl;
  }


  // Helper static function to print the packet's payload
  static void print_payload(const msg_payload_t& payload) {
    std::cout << "msg payload:" << std::endl;
    std::cout << "  timetag:" << std::hex << (payload.timetag & 0xFFFFFFFFFFFFFFFF) << std::endl;
    std::cout << "  command:" << std::hex << (payload.command & 0xFF) << std::endl;
    std::cout << "  floor number:" << std::hex << (payload.floor_num & 0xFF) << std::endl;
    std::cout << "  direction:" << std::hex << (payload.direction & 0xFF) << std::endl;
  }


  // Helper static function to handle the incoming packet from transport layer
  // It performs the following actions:
  //  - Parsing the packet header
  //  - Checking packet's sanity
  //  - Replying ACK/NACK to the transmitter
  //  - Parsing the packet payload
  // ToDo: This function returns a Request object. It might be a better
  //       strategy to easily pack the output data fields into a simple
  //       tuple!
  static Request handle(std::weak_ptr<TransportSocket::ClientSocket> socket, std::vector<uint8_t>& packet) {
	// Parse the packet header, check packet's sanity and reply ACK/NAK
    // Extract packet header
    std::istringstream stream(std::string((char*)&packet[0], packet.size()));
    stream.seekg(0, std::ios_base::beg);
    auto msg_header = MsgProtocol::deserialize_header(stream, true);
    print_header(msg_header);


    // Check packet
    bool packetCheck = header_check(msg_header, msg_header.len);

    uint16_t tx_node_addr = msg_header.tx_node_addr;
    uint16_t msg_id = msg_header.msg_id;


    // Prepare the replay packet
    auto tmp = msg_header.tx_node_addr;
    msg_header.tx_node_addr = msg_header.rx_node_addr;
    msg_header.rx_node_addr = tmp;

    if (packetCheck)  msg_header.msg_class = static_cast<msg_class_t>(static_cast<uint8_t>(MSGTYPE::MSG_CTRL) | static_cast<uint8_t>(MSG_OPTYPE::OP_ACK));
    else              msg_header.msg_class = static_cast<msg_class_t>(static_cast<uint8_t>(MSGTYPE::MSG_CTRL) | static_cast<uint8_t>(MSG_OPTYPE::OP_NAK));

    msg_header.len = sizeof(msg_hdr_t);

    // ////////////////////////////////////////////
    // Send back the reply packet as ACK/NAK
    // ToDo: Here it should be checked to optimize the code
    //       to avoid unnecessary memory copy.
    std::ostringstream ostream(std::ostringstream::ate);
    serialize_header(ostream, msg_header, true);
    std::vector<uint8_t> buffer;
    const std::string& str = ostream.str();
    buffer.insert(buffer.end(), str.begin(), str.end());
    socket.lock()->write(buffer);

    // ////////////////////////////////////////////
    // Parse the packet's payload
    stream.seekg(sizeof(msg_hdr_t), std::ios_base::beg);
    auto msg_payload = MsgProtocol::deserialize_payload(stream, true);
    print_payload(msg_payload);
    return (Request(tx_node_addr,
    		        msg_id,
					msg_payload.timetag,
					static_cast<Request::Command>(msg_payload.command),
					msg_payload.floor_num,
					static_cast<Request::Direction>(msg_payload.direction)));
  }


  // Helper static function to handle the incoming packet from transport layer
  // It performs the following actions:
  //  - Parsing the packet header
  //  - Checking packet's sanity
  //  - Replying ACK/NACK to the transmitter
  //  - Parsing the packet payload
  static void xmit(std::weak_ptr<TransportSocket::ClientSocket> socket, std::tuple<uint16_t, uint16_t, uint8_t, uint8_t, uint8_t>& cmd_tuple) {
    msg_hdr_t header;
    msg_payload_t payload;
    payload.timetag = 0xa;
    std::tie(header.rx_node_addr, header.msg_id, payload.command, payload.floor_num, payload.direction) = cmd_tuple;

    header.magic = MagicValue;
    header.tx_node_addr = NODE_ADDRESS;
    header.msg_class = 0x02;
    header.len = 23;

    // /////////////////////////////////////////////////////
    // Send back the reply packet as ACK/NAK
    // ToDo: This section needs to be optimized to avoid unnecessary
    //       memory copy!
    std::ostringstream ostream(std::ostringstream::ate);
    serialize_header(ostream, header, true);

    ostream.seekp(sizeof(msg_hdr_t), std::ios_base::beg);
    serialize_payload(ostream, payload, true);

    std::vector<uint8_t> buffer;
    const std::string& str = ostream.str();
    buffer.insert(buffer.end(), str.begin(), str.end());
    uint16_t crc = crc16(buffer);
    ostream.seekp(sizeof(msg_hdr_t)+sizeof(msg_payload_t), std::ios_base::beg);
    serialize_crc16(ostream, crc, true);

    {
    std::vector<uint8_t> buffer;
    const std::string& str = ostream.str();
    buffer.insert(buffer.end(), str.begin(), str.end());
    socket.lock()->write(buffer);
    }
  }

};



// Network protocol handler task which is derived from the stoppable thread for
// easy stopping. This task class handles the entire protocol stack and
// delivers the incoming user's requests to the elevator's core controller
// task via signal/slot design pattern.
class NetProtocol : public Stoppable {
private:

  // Signals and slots Observer Pattern which notifies the generation of a new OUTPUT DATA
  std::shared_ptr<signal_slot<std::tuple<uint16_t, uint16_t, uint8_t, uint8_t, uint8_t>&>> onNewData_;

  // Member variable for holding an instance of transport socket
  std::unique_ptr<TransportSocket> transportSocket_;

  // tuple type Output items vector from network protocol handler task
  std::tuple<uint16_t, uint16_t, uint8_t, uint8_t, uint8_t> output_items_;

  std::weak_ptr<TransportSocket::ClientSocket> socket_;
public:
  // ctor
  NetProtocol() : output_items_(std::make_tuple(0, 0, 0, 0, 0)) {
    onNewData_ = std::make_shared<signal_slot<std::tuple<uint16_t, uint16_t, uint8_t, uint8_t, uint8_t>&>>();
    transportSocket_ = std::unique_ptr<TransportSocket>(new TransportSocket(90000));
  }


  // dtor
  ~NetProtocol() {
    onNewData_ = nullptr;
    transportSocket_ = nullptr;
  }


  // Signals and slots Observer Pattern which emits a new output data
  void emitNewData() {
    onNewData_->emit(output_items_);
  }

  // Input callback method which is being called by the network layer as soon as
  // each input command request is being received
  void input_data_consumer(std::tuple<uint16_t, uint16_t, uint8_t, uint8_t, uint8_t>& cmd_tuple) {
    uint8_t cmd, floor_num, status;
    uint16_t node_addr, msg_id;
    std::tie(node_addr, msg_id, cmd, floor_num, status) = cmd_tuple;
    std::cout << "NetProtocol: input_data_consumer: (" << (cmd&0xFF) << "," << (floor_num&0xFF) << "," << (status&0xFF) << ")" << std::endl;
    MsgProtocol::xmit(socket_, cmd_tuple);
  }

  // Getter interface for the new DATA Signal/Slot
  std::shared_ptr<signal_slot<std::tuple<uint16_t, uint16_t, uint8_t, uint8_t, uint8_t>&>> getOnNewDataGen() { return onNewData_; };


  // Thread loop method
  void run() {
    std::cout << "Net Application Starting..." << std::endl;

    // Defining the onAccept callback for transport socket
    transportSocket_->onAccept( [&] ( std::weak_ptr<TransportSocket::ClientSocket> socket )
    {
  	  std::cout << "onAccept" << std::endl;

      if( auto s = socket.lock() ) {
        std::cout << "Connection accepted..." << std::endl;
        socket_ = socket;
//        s->close();
      }
    } );



    // Defining the onRead callback for transport socket
    transportSocket_->onRead( [&] ( std::weak_ptr<TransportSocket::ClientSocket> socket )
    {
      std::cout << "onRead" << std::endl;

      if( auto s = socket.lock() ) {
        std::vector<uint8_t> packet = s->read();

        // Printing the packet contents for debugging
        for (auto v : packet)
          std::cout << std::hex << (v & 0xFF) << " " << std::dec;
        std::cout << std::endl;

        // //////////////////////////////////////////////////////////////
        // Passing the received packet to Message Protocol class handler
        // ToDo: Here we receive the data from MsgProtocol::handle as a
        //       Request object and then we copy it into a tuple to emit it
        //       to the elevator's controller. This is an unnecessary operation
        //       which tasks time and resource. Hence, an optimization here should
        //       be done to use either tuple or Request for the whole scenario and
        //       instead of copy, use move concept.
        Request req = MsgProtocol::handle(s, packet);

        output_items_ = std::make_tuple(req.node_addr_,
                                        req.msg_id_,
       	                                static_cast<uint8_t>(req.cmd_),
        		                        req.floor_,
										static_cast<uint8_t>(req.direction_)); // call|go, floorNum, Up|Down
        std::cout << "NetProtocol: (" << std::hex << req.node_addr_ << "," << req.msg_id_ << "," << (static_cast<uint8_t>(req.cmd_)&0xFF) << "," << (req.floor_&0xFF) << "," << (static_cast<uint8_t>(req.direction_)&0xFF) << ")" << std::endl;

        // Emit the extracted user's request to the elevator's core controller
        emitNewData();

//        s->close();
      }
    } );

    auto function = [&]() -> bool { return stopRequested(); };
    // Invoking the transport socket listener method
    transportSocket_->listen(function);

    std::cout << "Net Application exits." << std::endl;
  }

};

}

#endif /* D_NET_PROTOCOL_H */
