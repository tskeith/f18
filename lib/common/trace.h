// Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FORTRAN_COMMON_TRACE_H_
#define FORTRAN_COMMON_TRACE_H_

#include <forward_list>
#include <list>
#include <sstream>
#include <string>
#include <vector>

namespace Fortran::common {

template<typename T>
std::ostream &operator<<(std::ostream &os, const std::list<T> &list) {
  if (list.empty()) {
    return os << "[]";
  } else {
    char sep{'['};
    for (const auto &x : list) {
      os << sep << x;
      sep = ',';
    }
    return os << ']';
  }
}

template<typename T>
std::ostream &operator<<(std::ostream &os, const std::vector<T> &vector) {
  if (vector.empty()) {
    return os << "[]";
  } else {
    char sep{'['};
    for (const auto &x : vector) {
      os << sep << x;
      sep = ',';
    }
    return os << ']';
  }
}

class Tracer {
public:
  Tracer() {}

  template<typename... A>
  void Trace(const char *file, int line, const char *func,
      const char *format = nullptr, A &&... args) {
    return DoTrace(
        file, line, func, format, ToCString(std::forward<A>(args))...);
  }

private:
  template<typename A> const char *ToCString(const A &x) {
    std::stringstream ss;
    ss << x;
    return ToCString(ss.str());
  }
  const char *ToCString(const std::string &);
  void DoTrace(const char *, int, const char *, const char *, ...);

  std::forward_list<std::string> conversions_;  // preserves created strings
};

#define TRACE(...) \
  Fortran::common::Tracer{}.Trace(__FILE__, __LINE__, __func__, __VA_ARGS__)

}
#endif
