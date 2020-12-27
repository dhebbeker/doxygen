/**
 * @file
 * Utilities for operation on std::vector.
 */

#ifndef VECTOR_UTILS_HPP
#define VECTOR_UTILS_HPP

#include <vector>
#include <type_traits>

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

template<typename T>
auto concat(const std::vector<T>& v, const T& t)
{
  std::remove_const_t<std::remove_reference_t<decltype(v)>> newV(v);
  newV.push_back(t);
  return newV;
}


#endif /* VECTOR_UTILS_HPP */
