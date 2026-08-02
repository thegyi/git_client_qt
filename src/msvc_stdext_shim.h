// msvc_stdext_shim.h
//
// Visual Studio 2022/2025/2026 (MSVC _MSC_VER >= 1938) removed the
// non-standard stdext::make_*_array_iterator helpers from <iterator>.
// Qt 6.5.x still references them through QT_MAKE_CHECKED_ARRAY_ITERATOR
// and QT_MAKE_UNCHECKED_ARRAY_ITERATOR, which causes a build failure.
//
// This header is force-included (/FI) on MSVC before any other include.
// It redefines those macros to the raw pointer, matching Qt's own
// non-MSVC fallback and avoiding the missing stdext namespace.

#pragma once

#if defined(_MSC_VER) && !defined(QT_MAKE_UNCHECKED_ARRAY_ITERATOR)
#define QT_MAKE_UNCHECKED_ARRAY_ITERATOR(x) (x)
#define QT_MAKE_CHECKED_ARRAY_ITERATOR(x, n) (x)
#endif
