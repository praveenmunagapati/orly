/* <orly/symbol/result_def.h>

   TODO

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

#include <base/class_traits.h>
#include <orly/symbol/def.h>

namespace Orly {

  namespace Symbol {

    class TAnyFunction;

    class TResultDef
        : public TDef,
          public std::enable_shared_from_this<TResultDef> {
      NO_COPY(TResultDef);
      public:

      typedef std::shared_ptr<TResultDef> TPtr;

      typedef std::shared_ptr<TAnyFunction> TAnyFunctionPtr;

      static TPtr New(const TAnyFunctionPtr &function, const std::string &name, const TPosRange &pos_range);

      virtual ~TResultDef();

      void Accept(const TVisitor &visitor) const;

      TAnyFunctionPtr GetFunction() const;

      Type::TType GetType() const;

      TAnyFunctionPtr TryGetFunction() const;

      private:

      TResultDef(const TAnyFunctionPtr &function, const std::string &name, const TPosRange &pos_range);

      std::weak_ptr<TAnyFunction> Function;

    };  // TResultDef

  }  // Symbol

}  // Orly
