/**
 * @file
 * Utilities for operation on std::vector.
 */

#ifndef VECTOR_UTILS_HPP
#define VECTOR_UTILS_HPP

#include <vector>

//! @todo https://stackoverflow.com/a/49174699/5534993
//! @todo https://en.cppreference.com/w/cpp/container/list/splice
//! @todo instead of using free functions to append to std::vector, insert the code inline?
//! @todo reduce the definitions of helper functions and use std functions instead

//! @see https://stackoverflow.com/a/21391109/5534993
template <typename T>
std::vector<T> operator+(const std::vector<T> &A, const std::vector<T> &B)
{
    std::vector<T> AB;
    AB.reserve( A.size() + B.size() );                // preallocate memory
    AB.insert( AB.end(), A.begin(), A.end() );        // add A;
    AB.insert( AB.end(), B.begin(), B.end() );        // add B;
    return AB;
}

 //! @see https://stackoverflow.com/a/21391109/5534993, https://stackoverflow.com/questions/2551775/appending-a-vector-to-a-vector#comment46026039_2551785
template <typename T>
std::vector<T> &operator+=(std::vector<T> &A, const std::vector<T> &B)
{
    A.reserve( A.size() + B.size() );                // preallocate memory without erase original data
    A.insert( A.end(), B.begin(), B.end() );         // add B;
    return A;                                        // here A could be named AB
}

template<typename T>
std::vector<T> operator+(const std::vector<T> &A, const T &B)
{
  std::vector<T> AB(A);
  AB.push_back(B);
  return AB;
}

template<typename T>
std::vector<T>& operator+=(std::vector<T> &A, const T &B)
{
  A.push_back(B);
  return A;
}

#endif /* VECTOR_UTILS_HPP */
