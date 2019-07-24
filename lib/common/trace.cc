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

#include "trace.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>

//TRACE
namespace Fortran::common {

const char *Tracer::ToCString(const std::string &s) {
  conversions_.emplace_front(s);
  return conversions_.front().c_str();
}

void Tracer::DoTrace(const char *file, int line, const char *format, ...) {
  const char *slash = std::strrchr(file, '/');
  fprintf(stderr, "%s:%d: ", slash ? slash + 1 : file, line);
  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  va_end(ap);
  fprintf(stderr, "\n");
}

}
