/*
 * @file   Elevator.h
 * @author Armin Zare Zadeh, ali.a.zarezadeh@gmail.com
 * @date   31 July 2020
 * @version 0.1
 * @brief   This file is intended to present a typical
 *          software system design for a typical elevator system.
 *          It's being designed based on multi-threading and observer
 *          design pattern.
 */

#ifndef D_ELEVATOR_H
#define D_ELEVATOR_H

#include "StoppableTask.h"
#include "signal_slot.h"
#include "NetProtocol.h"

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



// Return the number of milliseconds since the steady clock epoch. NOTE: The
// returned timestamp may be used for accurately measuring intervals but has
// no relation to wall clock time. It must not be used for synchronization
// across multiple nodes.
//
// \return The number of milliseconds since the steady clock epoch.
inline int64_t current_time_ms() {
  std::chrono::milliseconds ms_since_epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch());
  return ms_since_epoch.count();
}


// RAII method to safely join the thread
class ThreadJoiner {
  std::thread& m_th;
public:
  explicit ThreadJoiner(std::thread& t):m_th(t) {}
  ~ThreadJoiner() {
    if(m_th.joinable()) {
      m_th.join();
    }
  }
};


// Main Controller Class
// This class receives the data from the TCP/IP network handler class throughout
// signal/slot pattern. Internally, it processes the received commands from
// network in its process thread. The incoming request traffic is placed in
// different priority queues for an effective handling.
class ElevatorCtrl {
public:
  // State of the elevator whether moving or stopped
  enum class State : uint8_t { MOVING = 1, STOPPED };

  // State of the door; opened or closed
  enum class Door : uint8_t { OPEN = 1, CLOSED };

private:
  uint8_t location_;  // Location of elevator's car
  Request::Direction direction_; // Holds direction of moving (Up/Down)
  State state_; // Holds the state of the elevator (Moving/Stop)
  Door door_;   // Holds the state of the doors (Open/Close)

  // The following queue, mutex, and condition variable are used for
  // thread safe synchronization between pushing side of the incoming traffic
  // into input queue and consuming side of the controller.
  std::deque<int> inputQueue_;
  std::mutex inputQueueMutex_;
  std::condition_variable inputQueueCondVar_;

  // tuple type Output items vector from network protocol handler task
  std::tuple<uint16_t, uint16_t, uint8_t, uint8_t, uint8_t> output_items_;

public:
  // ctor
  ElevatorCtrl() : location_(0),
                   direction_(Request::Direction::UP),
				   state_(State::STOPPED),
				   door_(Door::CLOSED),
				   output_items_(std::make_tuple(0, 0, 0, 0, 0)) {
    onNewData_ = std::make_shared<signal_slot<std::tuple<uint16_t, uint16_t, uint8_t, uint8_t, uint8_t>&>>();
  }

  // dtor
  ~ElevatorCtrl() {
    onNewData_ = nullptr;
  }

  // Signals and slots Observer Pattern which emits a new output data to the network protocol subsystem
  void emitNewData() {
    onNewData_->emit(output_items_);
  }

  // Getter interface for the new DATA Signal/Slot
  std::shared_ptr<signal_slot<std::tuple<uint16_t, uint16_t, uint8_t, uint8_t, uint8_t>&>> getOnNewDataGen() { return onNewData_; };

private:
  // priority queues for prioritizing the handling of input traffic
  std::priority_queue<Request, std::vector<Request>, upComparator> upQueue_;
  std::priority_queue<Request, std::vector<Request>, downComparator> downQueue_;
  std::queue<Request> currentQueue_;

  // Signals and slots Observer Pattern which notifies the generation of a new OUTPUT DATA
  std::shared_ptr<signal_slot<std::tuple<uint16_t, uint16_t, uint8_t, uint8_t, uint8_t>&>> onNewData_;

public:
  // Input callback method which is being called by the network layer as soon as
  // each input command request is being received
  void input_data_consumer(std::tuple<uint16_t, uint16_t, uint8_t, uint8_t, uint8_t>& cmd_tuple) {
    uint8_t cmd, floor_num, direction;
    uint16_t node_addr, msg_id;
    std::tie(node_addr, msg_id, cmd, floor_num, direction) = cmd_tuple;
    std::cout << "input_data_consumer: (" << node_addr << "," << msg_id << "," << (cmd&0xFF) << "," << (floor_num&0xFF) << "," << (direction&0xFF) << ")" << std::endl;
    switch (cmd) {
      case 1: // call
        call(node_addr, msg_id, floor_num, static_cast<Request::Direction>(direction));
        break;
      case 2: // go;
        go(node_addr, msg_id, floor_num);
        break;
      default: // code to be executed if n doesn't match any cases
        throw std::invalid_argument("Illegal command: " + std::to_string(cmd));
    }
  }

private:

  // This method is being invoked based on each "call" command request
  // by user
  void call(uint16_t node_addr, uint16_t msg_id, uint8_t floor, Request::Direction direction) {
    std::unique_lock<std::mutex> locker(inputQueueMutex_);

    if (direction == Request::Direction::UP) {
      if (floor >= location_) {
        currentQueue_.push(Request(node_addr, msg_id, current_time_ms(), static_cast<Request::Command>(0), floor, direction));
      }
      else {
        upQueue_.push(Request(node_addr, msg_id, current_time_ms(), static_cast<Request::Command>(0), floor, direction));
      }
    } else {
      if (floor <= location_) {
        currentQueue_.push(Request(node_addr, msg_id, current_time_ms(), static_cast<Request::Command>(0), floor, direction));
      } else {
        downQueue_.push(Request(node_addr, msg_id, current_time_ms(), static_cast<Request::Command>(0), floor, direction));
      }
    }

    locker.unlock();
    inputQueueCondVar_.notify_one();  // Notify one waiting thread, if there is one.
  }


  // This method is being invoked based on each "go" command request
  // by user
  void go(uint16_t node_addr, uint16_t msg_id, uint8_t floor) {
    call(node_addr, msg_id, floor, direction_);
  }


  // The process functor which is being called by internal thread
  void process() {
    auto sec = std::chrono::seconds(1);
    Request r(0,0,0,static_cast<Request::Command>(0),0,static_cast<Request::Direction>(0));
    bool gotToken = false;
    std::unique_lock<std::mutex> locker(inputQueueMutex_);
    inputQueueCondVar_.wait_for(locker, 2*sec, [&]() -> bool { return !upQueue_.empty() || !downQueue_.empty() || !currentQueue_.empty();} );  // Unlock mu and wait to be notified

    if (!currentQueue_.empty()) {
      r = currentQueue_.front();
      currentQueue_.pop();
      gotToken = true;
    } else {
      preProcessNextQueue();
    }
    locker.unlock();
    if (gotToken) goToFloor(r.node_addr_, r.msg_id_, r.floor_);
  }


  // This method simulates the actor which moves the elevator up/down and
  // opens/closes the doors. By using some delays it simulates the physical
  // nature of the elevator
  void goToFloor(uint16_t node_addr, uint16_t msg_id, uint8_t floor) {
    std::cout << "goToFloor: moving to " << (floor&0xFF) << std::endl;
    state_ = State::MOVING;
    for (uint8_t i = location_; i <= floor; i++) {
      // Simulate the time which the elevator's car spends to traverse between floors
      std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds(1));

      ///////////////////////////////////////////////
      // Sending the current status to the requester
      output_items_ = std::make_tuple(node_addr, msg_id, 3, i, static_cast<uint8_t>(State::MOVING)); // status, floorNum, moving
      // Emit the status request to the network protocol subsystem
      emitNewData();
    }
    location_ = floor;
    door_ = Door::OPEN;
    state_ = State::STOPPED;

    ///////////////////////////////////////////////
    // Sending the current status to the requester
    output_items_ = std::make_tuple(node_addr, msg_id, 3, floor, static_cast<uint8_t>(State::STOPPED)); // status, floorNum, stop
    // Emit the status request to the network protocol subsystem
    emitNewData();

    // Simulate the time which the elevator's car stays at the destination
    std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds(3));
    door_ = Door::CLOSED;

    std::cout << "goToFloor: reached to " << floor << std::endl;
  }

private:
  // This method prepares the current queue based on moving the content of
  // up/down priority queue into the normal queue. Basically, the algorithm
  // beneath this method tries to get service to all the requests in the
  // current direction of the movement.
  void preProcessNextQueue() {
    if (getLowestTimeUpQueue() > getLowestTimeDownQueue()) {
      direction_ = Request::Direction::UP;
      while (!upQueue_.empty()) {
    	currentQueue_.push(std::move(const_cast<Request&>(upQueue_.top())));
        upQueue_.pop();
      }
    } else {
      direction_ = Request::Direction::DOWN;
      while (!downQueue_.empty()) {
    	currentQueue_.push(std::move(const_cast<Request&>(downQueue_.top())));
    	downQueue_.pop();
      }
    }
  }


  // This method returns the time tag of the element in the Up Queue
  // with the least time tag
  int64_t getLowestTimeUpQueue() {
	int64_t lowest = LONG_MAX;
    while (!upQueue_.empty()) {
      if (upQueue_.top().time_ < lowest)
        lowest = upQueue_.top().time_;
    }
    return lowest;
  }


  // This method returns the time tag of the element in the Down Queue
  // with the least time tag
  int64_t getLowestTimeDownQueue() {
	int64_t lowest = LONG_MAX;
    while (!downQueue_.empty()) {
      if (downQueue_.top().time_ < lowest)
        lowest = downQueue_.top().time_;
    }
    return lowest;
  }


  // Process task which is derived from the stoppable thread for
  // easy stopping
  class Process : public Stoppable {
  private:
    ElevatorCtrl* parent_;

  public:
    // ctor
    Process(ElevatorCtrl* parent) : parent_(parent) {}

    // dtor
    ~Process() {}

    // Thread loop method
    void run() {
      std::cout << "ElevatorCtrl Process Start" << std::endl;
      while (stopRequested() == false) {
        parent_->process();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      std::cout << "ElevatorCtrl Process End" << std::endl;
    }
  };


  // Task process variables
  std::unique_ptr<ElevatorCtrl::Process> taskProcess;
  std::thread processingThread;

public:
  // Helper method for creating the process thread
  void make_process_thread() {
    if (taskProcess) return;

    taskProcess = std::unique_ptr<ElevatorCtrl::Process>(new ElevatorCtrl::Process(this));
    std::cout << "Starting elevator controller processing task..." << std::endl;
    processingThread = std::thread([&]()
    {
      taskProcess->run();
    });
  }


  // Helper method for stopping the process thread
  void stop_process_thread() {
    if(processingThread.joinable()) { if (taskProcess) taskProcess->stop(); }
  }


  // Wrapper method to join the processing thread
  void join_process_thread() {
    ThreadJoiner processingThreadJoin(processingThread);
  }
};



// The main elevator class which creates the whole system including the
// controller and the network classes.
class Elevator : noncopyable
{
private:
  // Elevator controller
  std::shared_ptr<ElevatorCtrl> elevatorCtrl;
  // Network handler
  std::shared_ptr<Net::NetProtocol> taskNetProtocol;
  std::thread netProtocolThread;

public:

  // ctor
  Elevator(const char* cfg_file_name) {
    elevatorCtrl = std::shared_ptr<ElevatorCtrl>(new ElevatorCtrl());
    taskNetProtocol = std::shared_ptr<Net::NetProtocol>(new Net::NetProtocol());
  }


  // dtor
  virtual ~Elevator() {
    std::cout << "Dtor the elevator system..." << std::endl;
    stop();

    if (taskNetProtocol) taskNetProtocol = nullptr;
    if (elevatorCtrl) elevatorCtrl = nullptr;
  }


  // Helper method to connect the signal and slot methods in the
  // network and controller sub-classes
  void connect_signal_slot() {
    taskNetProtocol->getOnNewDataGen()->connect_member<ElevatorCtrl>(elevatorCtrl, &ElevatorCtrl::input_data_consumer);
    elevatorCtrl->getOnNewDataGen()->connect_member<Net::NetProtocol>(taskNetProtocol, &Net::NetProtocol::input_data_consumer);
  }


  // Main routine to run the elevator system
  void run() {
    std::cout << "Starting the elevator system..." << std::endl;
    elevatorCtrl->make_process_thread();
    netProtocolThread = std::thread([&]()
    {
      taskNetProtocol->run();
    });

    elevatorCtrl->join_process_thread();
    ThreadJoiner netProtocolThreadJoin(netProtocolThread);

    std::cout << "Exiting the elevator system." << std::endl;
  }


  // Routine to stop the elevator system
  void stop() {
    std::cout << "Stopping the elevator system..." << std::endl;
    if(netProtocolThread.joinable()) { if (taskNetProtocol) taskNetProtocol->stop(); }
    if (elevatorCtrl) elevatorCtrl->stop_process_thread();
  }
};


#endif /* D_ELEVATOR_H */
