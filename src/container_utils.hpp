/**
 * @file
 * Utilities for operation on containers.
 */

#ifndef CONTAINER_UTILS_HPP
#define CONTAINER_UTILS_HPP

#include <type_traits>

namespace details {

// Nice syntax to allow in-order expansion of parameter packs.
struct do_in_order {
    template<typename T> do_in_order(std::initializer_list<T>&&) { }
};

template<typename Container> void concat_helper(Container& originalContainer, const Container& additionalContainer) {
    originalContainer.insert(std::end(originalContainer), std::begin(additionalContainer), std::end(additionalContainer));
}

template<class Container> void concat_helper(Container& originalContainer, Container&& additionalContainer) {
    originalContainer.insert(std::end(originalContainer), std::make_move_iterator(std::begin(additionalContainer)),
             std::make_move_iterator(std::end(additionalContainer)));
}
} // namespace details

template<typename Container, typename... OtherContainers>
auto concat(Container containerCopy, OtherContainers&&... additionalContainers) {
    std::size_t accumulatedSize = containerCopy.size();
    details::do_in_order { accumulatedSize += additionalContainers.size() ... };
    containerCopy.reserve(accumulatedSize);
    details::do_in_order { (details::concat_helper(containerCopy, std::forward<OtherContainers>(additionalContainers)), 0)... };
    return std::move(containerCopy);   // rvo blocked
}

template<template<typename, typename> class ContainerType, typename ValueType, typename Allocator>
auto concat(const ContainerType<ValueType, Allocator>& originalContainer, const ValueType& additionalValue)
{
  std::remove_const_t<std::remove_reference_t<decltype(originalContainer)>> newContainer(originalContainer);
  newContainer.push_back(additionalValue);
  return newContainer;
}


#endif /* CONTAINER_UTILS_HPP */
