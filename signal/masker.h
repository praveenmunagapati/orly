/* <signal/masker.h>

   RAII for setting the signal mask.

   Copyright 2010-2014 OrlyAtomics, Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#pragma once

#include <cassert>

#include <signal.h>

#include <base/class_traits.h>
#include <util/error.h>

namespace Signal {

  /* RAII for setting the signal mask. */
  class TMasker {
    NO_COPY(TMasker);
    public:

    /* Set the mask to the given set. */
    TMasker(const sigset_t &new_set) {
      Util::IfNe0(pthread_sigmask(SIG_SETMASK, &new_set, &OldSet));
    }

    /* Restore the mask. */
    ~TMasker() {
      assert(this);
      pthread_sigmask(SIG_SETMASK, &OldSet, nullptr);
    }

    const sigset_t &GetOldSet() const {
      return OldSet;
    }

    private:

    /* The set to which we will restore. */
    sigset_t OldSet;

  };  // TMasker

}  // Signal
