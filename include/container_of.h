#pragma once
#include <cstddef>

template <typename T, typename M>
constexpr std::size_t offset_of(M T::*member) {
  // Gets the offset of the member relative to the container
  return reinterpret_cast<std::size_t>(&(reinterpret_cast<T *>(0)->*member));
}

template <typename T, typename M> T *container_of(M *ptr, M T::*member) {
  // Gets the address of the container using its member by calculating the
  // offset
  return reinterpret_cast<T *>(reinterpret_cast<char *>(ptr) -
                               offset_of(member));
}
