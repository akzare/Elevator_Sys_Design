/**
 * @file   signal_slot.h
 * @author Originally, this file is from:
 *           https://schneegans.github.io/tutorials/2015/09/20/signal-slot.html
 * @date   12 Mar 2020
 * @version 0.1
 * @brief   Implements the observer design pattern
 */

#ifndef SIGNAL_SLOT_H
#define SIGNAL_SLOT_H

#include <functional>
#include <map>
#include <memory>



// A signal object may call multiple slots with the
// same signature. You can connect functions to the signal
// which will be called when the emit() method on the
// signal object is invoked. Any argument passed to emit()
// will be passed to the given functions.

template <typename... Args>
class signal_slot {

 public:

  signal_slot() : current_id_(0) {}

  // copy creates new signal
  signal_slot(signal_slot const& other) : current_id_(0) {}

  // connects a member function to this Signal
  template <typename T>
  int connect_member(T *inst, void (T::*func)(Args...)) {
    return connect([=](Args... args) {
      (inst->*func)(args...);
    });
  }

  // connects a const member function to this Signal
  template <typename T>
  int connect_member(T *inst, void (T::*func)(Args...) const) {
    return connect([=](Args... args) {
      (inst->*func)(args...);
    });
  }

  // connects a member function to this Signal (shared pointer)
  template <typename T>
  int connect_member(std::shared_ptr<T> inst, void (T::*func)(Args...)) {
    return connect([=](Args... args) {
//      (inst->*func)(args...);
   	  T *inst_raw = inst.get();
   	  (inst_raw->*func)(args...);
    });
  }


  // connects a const member function to this Signal  (shared pointer)
  template <typename T>
  int connect_member(std::shared_ptr<T> inst, void (T::*func)(Args...) const) {
    return connect([=](Args... args) {
//      (inst->*func)(args...);
      T *inst_raw = inst.get();
      (inst_raw->*func)(args...);
    });
  }

  // connects a std::function to the signal. The returned
  // value can be used to disconnect the function again
  int connect(std::function<void(Args...)> const& slot) const {
    slots_.insert(std::make_pair(++current_id_, slot));
    return current_id_;
  }

  // disconnects a previously connected function
  void disconnect(int id) const {
    slots_.erase(id);
  }

  // disconnects all previously connected functions
  void disconnect_all() const {
    slots_.clear();
  }

  // calls all connected functions
  void emit(Args... p) {
    for(auto it : slots_) {
      it.second(p...);
    }
  }

  // assignment creates new Signal
  signal_slot& operator=(signal_slot const& other) {
    disconnect_all();
  }

 private:
  mutable std::map<int, std::function<void(Args...)>> slots_;
  mutable int current_id_;
};


#endif // SIGNAL_SLOT_H
