/**
 * @file   NonCopyable.h
 * @author Armin Zare Zadeh, ali.a.zarezadeh@gmail.com
 * @date   12 Mar 2020
 * @version 0.1
 * @brief   Includes the implementation for a typical non-copyable class
 */

#ifndef NONCOPYABLE_H
#define NONCOPYABLE_H


// Inherit this class to create a noncopyable object
class noncopyable
{
public:
  // Switched to C++11 way of doing it
  noncopyable(const noncopyable&) = delete;

  // Switched to C++11 way of doing it
  noncopyable& operator=(const noncopyable&) = delete;

protected:
  noncopyable() = default;
};

#endif // NONCOPYABLE_H
