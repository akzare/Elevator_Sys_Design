/*
 * @file   ElevatorTest.py
 * @author Armin Zare Zadeh, ali.a.zarezadeh@gmail.com
 * @date   31 July 2020
 * @version 0.1
 * @brief   An elevator system design unit test.
 */

#include <gtest\gtest.h>
#include <Elevator.h>

#include <chrono>
#include <random>
#include <list>
#include <queue>
#include <iterator>
#include <vector>
#include <algorithm>

namespace dsa {

class ElevatorTest : public ::testing::Test {

protected:

  std::unique_ptr<Elevator> elevator;

  ElevatorTest() {
    elevator = std::unique_ptr<Elevator>(new Elevator(""));
  }

  virtual ~ElevatorTest() {
    elevator = nullptr;
  }

  virtual void SetUp() {

  }

  virtual void TearDown() {

  }
};



// Functor helper class to stop the elevator in another thread while it is running
class Fctor {
  std::function<void ()> stopRequest_;
public:
  Fctor(std::function<void ()> stopRequest):stopRequest_(stopRequest) {}
  void operator()(int64_t delay) {
    std::cout << "Fctor: Running Functor after: " << delay << "ms" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    stopRequest_();
    std::cout << "Fctor: Done" << std::endl;
  }
};


TEST_F(ElevatorTest, testSafeExiting) {
  elevator->connect_signal_slot();
  auto function = [&]() -> void { elevator->stop(); };
  std::thread t1((Fctor(function)), 20000);
  elevator->run();

  ThreadJoiner tj(t1);

  std::cout << "ElevatorTest: run done" << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::cout << "ElevatorTest: testSafeExiting done" << std::endl;
}


} // namespace dsa
