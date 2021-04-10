// Copyright 2021 Chia Network Inc

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//    http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_CPP_PROGRESS_HPP_
#define SRC_CPP_PROGRESS_HPP_

#include <functional>

#include <util.hpp>

typedef std::function<void(int,int,int)> ProgressCallbackFunc;

static ProgressCallbackFunc progressCallbackDefault{[](int phase, int n, int max_n){
    float p = (100.0 / 4) * ((phase - 1.0) + (1.0 * n / max_n));
    Util::Log("Progress: %s%%\n", static_cast<int64_t>(p));
}};
static ProgressCallbackFunc progressCallbackNone{[](int, int, int){}};

#endif  // SRC_CPP_PROGRESS_HPP
