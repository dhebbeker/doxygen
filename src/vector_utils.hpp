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

// Nice syntax to allow in-order expansion of parameter packs.
struct do_in_order {
    template<typename T> do_in_order(std::initializer_list<T>&&) { }
};

namespace details {
template<typename V> void concat_helper(V& l, const V& r) {
    l.insert(l.end(), r.begin(), r.end());
}
template<class V> void concat_helper(V& l, V&& r) {
    l.insert(l.end(), std::make_move_iterator(r.begin()),
             std::make_move_iterator(r.end()));
}
} // namespace details

template<typename T, typename... A>
std::vector<T> concat(std::vector<T> v1, A&&... vr) {
    std::size_t s = v1.size();
    do_in_order { s += vr.size() ... };
    v1.reserve(s);
    do_in_order { (details::concat_helper(v1, std::forward<A>(vr)), 0)... };
    return std::move(v1);   // rvo blocked
}


#endif /* VECTOR_UTILS_HPP */
