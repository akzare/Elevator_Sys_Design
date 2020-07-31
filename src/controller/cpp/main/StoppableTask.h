/*
 * @file   StoppableTask.h
 * @author C++11 : How to Stop or Terminate a Thread:
 *           https://thispointer.com/c11-how-to-stop-or-terminate-a-thread/
 * @date   27 July 2020
 * @version 0.1
 * @brief   Implements a C++11 stoppable thread class.
 */

#ifndef D_STOPPABLETASK_H
#define D_STOPPABLETASK_H


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



class Stoppable
{
private:
  std::promise<void> exitSignal;
  std::future<void> futureObj;
public:
  Stoppable(): futureObj(exitSignal.get_future()) {}
  Stoppable(Stoppable && obj): exitSignal(std::move(obj.exitSignal)), futureObj(std::move(obj.futureObj)) {}

  Stoppable & operator=(Stoppable && obj)
  {
    exitSignal = std::move(obj.exitSignal);
    futureObj = std::move(obj.futureObj);
    return *this;
  }

  virtual ~Stoppable() {  }

  // Task need to provide definition for this function
  // It will be called by thread function
  virtual void run() = 0;

  // Thread function to be executed by thread
  void operator()()
  {
    run();
  }


  //Checks if thread is requested to stop
  bool stopRequested()
  {
    // checks if value in future object is available
    if (futureObj.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout)
      return false;
    return true;
  }


  // Request the thread to stop by setting value in promise object
  void stop()
  {
    exitSignal.set_value();
  }
};


#endif /* D_STOPPABLETASK_H */
