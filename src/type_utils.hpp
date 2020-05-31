/**
 * @file
 * Utilities to perform miscellaneous type transformations.
 */

#ifndef TYPE_UTILS_HPP
#define TYPE_UTILS_HPP

#include <type_traits>
#include <memory>

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
template<template<typename, typename> class ContainerType, typename ValueType, typename Allocator>
struct MakeConstValueTypeContainerHelper<ContainerType<ValueType*, Allocator> >
{
  /** new value type of the cloned container */
  typedef std::add_pointer_t<std::add_const_t<ValueType> > NewValueType;

  /** new container type */
  using NewContainerType = ContainerType< NewValueType, std::allocator<NewValueType> >;
};

/** Clones a container type and add `const T` to value type `T`.
 *
 * @tparam Container is the container type, which has values of type `T`.
 *  */
template<class Container>
using MakeConstValueTypeContainer = typename MakeConstValueTypeContainerHelper<Container>::NewContainerType;

/** @}*/ // end group Group_MakeConstValueTypeContainer

/**
 * Create a copy of sourceContainer to a new container with the value type `const` qualified.
 * @param sourceContainer original container
 * @return clone of original container with `const` qualified values
 */
template<class Container>
auto makeConstCopy(const Container& sourceContainer)
{
  MakeConstValueTypeContainer<Container> constContainer(sourceContainer.size());
  for(const auto element : sourceContainer)
  {
    constContainer.push_back(element);
  }
  return constContainer;
}

#endif /* TYPE_UTILS_HPP */
