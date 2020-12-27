/**
 * @file
 * Utilities to perform miscellaneous type transformations and operations on containers.
 */

#ifndef CONTAINER_UTILS_HPP
#define CONTAINER_UTILS_HPP

#include <algorithm>
#include <type_traits>
#include <initializer_list>
#include <memory>

namespace details
{

// Nice syntax to allow in-order expansion of parameter packs.
struct do_in_order
{
  template<typename T> do_in_order(std::initializer_list<T>&&)
  {
  }
};

template<typename Container> void concat_helper(Container &originalContainer, const Container &additionalContainer)
{
  originalContainer.insert(std::end(originalContainer), std::begin(additionalContainer), std::end(additionalContainer));
}

template<class Container> void concat_helper(Container &originalContainer, Container &&additionalContainer)
{
  originalContainer.insert(
                           std::end(originalContainer),
                           std::make_move_iterator(std::begin(additionalContainer)),
                           std::make_move_iterator(std::end(additionalContainer)));
}

/** @defgroup Group_MakeConstValueTypeContainer Helper to clone a container type and add `const T` to value type `T`.
 *
 * The same class template for the container type is used.
 *
 * @{
 */

template<class Container> struct MakeConstValueTypeContainerHelper;

/** Defines a container with the value type `const T*` based on a container with value type `T*`.
 *
 * The allocator type of the new container is `std::allocator<NewValueType>`.
 *
 * @tparam ContainerType is any container type taking a value type and and allocator type as template arguments.
 * @tparam ValueType is the type pointed to by the container ("`value_type*`")
 * @tparam Allocator is not used
 *  */
template<template<typename, typename > class ContainerType, typename ValueType, typename Allocator>
struct MakeConstValueTypeContainerHelper<ContainerType<ValueType*, Allocator> >
{
  /** new value type of the cloned container */
  typedef std::add_pointer_t<std::add_const_t<ValueType> > NewValueType;

  /** new container type */
  using NewContainerType = ContainerType< NewValueType, std::allocator<NewValueType> >;
};

/** @}*/ // end group Group_MakeConstValueTypeContainer
}// namespace details

template<typename Container, typename ... OtherContainers>
auto concat(Container containerCopy, OtherContainers &&... additionalContainers)
{
  std::size_t accumulatedSize = containerCopy.size();
  details::do_in_order { accumulatedSize += additionalContainers.size() ... };
  containerCopy.reserve(accumulatedSize);
  details::do_in_order { (details::concat_helper(containerCopy, std::forward<OtherContainers>(additionalContainers)), 0)... };
  return std::move(containerCopy);   // rvo blocked
}

template<template<typename, typename > class ContainerType, typename ValueType, typename Allocator>
auto concat(const ContainerType<ValueType, Allocator> &originalContainer, const ValueType &additionalValue)
{
  std::remove_const_t<std::remove_reference_t<decltype(originalContainer)>> newContainer(originalContainer);
  newContainer.push_back(additionalValue);
  return newContainer;
}

/** Clones a container type and add `const T` to value type `T`.
 *
 * @ingroup Group_MakeConstValueTypeContainer
 * @tparam Container is the container type, which has values of type `T`.
 *  */
template<class Container>
using MakeConstValueTypeContainer = typename details::MakeConstValueTypeContainerHelper<Container>::NewContainerType;

/**
 * Create a copy of sourceContainer to a new container with the value type `const` qualified.
 * @param sourceContainer original container
 * @return clone of original container with `const` qualified values
 */
template<class Container>
auto makeConstCopy(const Container &sourceContainer)
{
  MakeConstValueTypeContainer<Container> constContainer;
  constContainer.insert(constContainer.end(), sourceContainer.begin(), sourceContainer.end());
  return constContainer;
}

/**
 * Creates copy without duplicate values.
 * @attention original order is not preserved
 * @param originalValues
 * @return
 */
template<typename Container>
Container removeDuplicates(Container originalValues)
{
  std::sort(originalValues.begin(), originalValues.end());
  const auto last = std::unique(originalValues.begin(), originalValues.end());
  originalValues.erase(last, originalValues.end());
  return originalValues;
}

#endif /* CONTAINER_UTILS_HPP */
