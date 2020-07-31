/*
 * @file   Request.h
 * @author Armin Zare Zadeh, ali.a.zarezadeh@gmail.com
 * @date   04 July 2020
 * @version 0.1
 * @brief   This file implements the Request class for modeling
 *          a typical user request from elevator's user.
 */

#ifndef D_ELEVATOR_REQUEST_H
#define D_ELEVATOR_REQUEST_H


#include <deque>
#include <queue>
#include <vector>
#include <set>   // set and multiset
#include <map>   // map and multimap
#include <unordered_set>  // unordered set/multiset
#include <unordered_map>  // unordered map/multimap
#include <iterator>
#include <algorithm>
#include <numeric>    // some numeric algorithm
#include <functional>
#include <stack>
#include <limits>
#include <thread>
#include <future>
#include <sstream>
#include <memory>
#include <iostream>
#include <stdexcept>

#include <chrono>


// This class represents the incoming request element from user.
// It encapsulates the timetag, command, floor number, and direction.
class Request {
public:
  // Command type
  enum class Command : uint8_t { CALL = 1, GO };
  // Direction type
  enum class Direction : uint8_t { UP = 1, DOWN };

  uint16_t node_addr_; // Requester Node Address
  int64_t time_; // Time tag
  uint16_t msg_id_; // Network Message ID
  Command cmd_; // Command
  uint8_t floor_; // floor number
  Direction direction_; // direction of movement
  bool ok_; // Indicates whether this request is a valid request


  // ctor
  Request(uint16_t node_addr, uint16_t msg_id, long time, Command cmd, uint8_t floor, Direction direction, bool ok = true) : node_addr_(node_addr), msg_id_(msg_id), time_(time), cmd_(cmd), floor_(floor), direction_(direction), ok_(ok) {}

  // Comparing two instances of this class based on their time tag
  static inline bool timetag_compare(const Request& x, const Request& y) {
    return x.time_ < y.time_;
  }


  // Operator == overloading
  inline bool operator==(const Request& t) const {
    return (t.node_addr_ == node_addr_) &&
           (t.msg_id_ == msg_id_) &&
           (t.cmd_ == cmd_) &&
           (t.direction_ == direction_) &&
           (t.floor_ == floor_) &&
           (t.time_ == time_);
  }


  Request(const Request& rhs)
        : node_addr_(rhs.node_addr_),
		  msg_id_(rhs.msg_id_),
		  time_(rhs.time_),
		  cmd_(rhs.cmd_),
		  floor_(rhs.floor_),
		  direction_(rhs.direction_),
		  ok_(rhs.ok_)
  {
  }


  Request& operator=(const Request& rhs) {
    if (this != &rhs) {
      node_addr_ = rhs.node_addr_;
   	  msg_id_ = rhs.msg_id_;
  	  time_ = rhs.time_;
  	  cmd_ = rhs.cmd_;
  	  floor_ = rhs.floor_;
  	  direction_ = rhs.direction_;
    }
    return (*this);
  }

};


struct upComparator {
  bool operator()(Request const & p1, Request const & p2) {
    // return "true" if "p1" is ordered before "p2", for example:
    return p1.floor_ < p2.floor_;
  }
};


struct downComparator {
  bool operator()(Request const & p1, Request const & p2) {
    // return "true" if "p1" is ordered before "p2", for example:
    return p1.floor_ >= p2.floor_;
  }
};

#endif /* D_ELEVATOR_REQUEST_H */
