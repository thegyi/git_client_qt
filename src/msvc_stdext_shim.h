// msvc_stdext_shim.h
//
// Newer MSVC runtimes no longer provide the stdext::make_*_array_iterator
// helpers that Qt 6.5.x still references through QT_MAKE_CHECKED_ARRAY_ITERATOR
// and QT_MAKE_UNCHECKED_ARRAY_ITERATOR.
//
// This header is force-included (/FI) on MSVC before any other include.

#pragma once

#if defined(_MSC_VER)
#include <cstddef>

#if _MSC_VER >= 1938
namespace stdext {
template <typename T> T *make_unchecked_array_iterator(T *ptr) { return ptr; }

template <typename T> T *make_checked_array_iterator(T *ptr, std::size_t) {
  return ptr;
}

template <typename T>
T *make_checked_array_iterator(T *ptr, std::size_t, std::size_t = 0) {
  return ptr;
}
} // namespace stdext
#else
#include <iterator>
#endif

#endif
