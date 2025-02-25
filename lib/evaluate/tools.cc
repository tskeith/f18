// Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include "tools.h"
#include "traversal.h"
#include "../common/idioms.h"
#include "../parser/message.h"
#include <algorithm>
#include <variant>

using namespace Fortran::parser::literals;

namespace Fortran::evaluate {

// IsVariable()
void IsVariableVisitor::Handle(const ProcedureDesignator &x) {
  if (const semantics::Symbol * symbol{x.GetSymbol()}) {
    Return(symbol->attrs().test(semantics::Attr::POINTER));
  } else {
    Return(false);
  }
}

// Conversions of complex component expressions to REAL.
ConvertRealOperandsResult ConvertRealOperands(
    parser::ContextualMessages &messages, Expr<SomeType> &&x,
    Expr<SomeType> &&y, int defaultRealKind) {
  return std::visit(
      common::visitors{
          [&](Expr<SomeInteger> &&ix,
              Expr<SomeInteger> &&iy) -> ConvertRealOperandsResult {
            // Can happen in a CMPLX() constructor.  Per F'2018,
            // both integer operands are converted to default REAL.
            return {AsSameKindExprs<TypeCategory::Real>(
                ConvertToKind<TypeCategory::Real>(
                    defaultRealKind, std::move(ix)),
                ConvertToKind<TypeCategory::Real>(
                    defaultRealKind, std::move(iy)))};
          },
          [&](Expr<SomeInteger> &&ix,
              Expr<SomeReal> &&ry) -> ConvertRealOperandsResult {
            return {AsSameKindExprs<TypeCategory::Real>(
                ConvertTo(ry, std::move(ix)), std::move(ry))};
          },
          [&](Expr<SomeReal> &&rx,
              Expr<SomeInteger> &&iy) -> ConvertRealOperandsResult {
            return {AsSameKindExprs<TypeCategory::Real>(
                std::move(rx), ConvertTo(rx, std::move(iy)))};
          },
          [&](Expr<SomeReal> &&rx,
              Expr<SomeReal> &&ry) -> ConvertRealOperandsResult {
            return {AsSameKindExprs<TypeCategory::Real>(
                std::move(rx), std::move(ry))};
          },
          [&](Expr<SomeInteger> &&ix,
              BOZLiteralConstant &&by) -> ConvertRealOperandsResult {
            return {AsSameKindExprs<TypeCategory::Real>(
                ConvertToKind<TypeCategory::Real>(
                    defaultRealKind, std::move(ix)),
                ConvertToKind<TypeCategory::Real>(
                    defaultRealKind, std::move(by)))};
          },
          [&](BOZLiteralConstant &&bx,
              Expr<SomeInteger> &&iy) -> ConvertRealOperandsResult {
            return {AsSameKindExprs<TypeCategory::Real>(
                ConvertToKind<TypeCategory::Real>(
                    defaultRealKind, std::move(bx)),
                ConvertToKind<TypeCategory::Real>(
                    defaultRealKind, std::move(iy)))};
          },
          [&](Expr<SomeReal> &&rx,
              BOZLiteralConstant &&by) -> ConvertRealOperandsResult {
            return {AsSameKindExprs<TypeCategory::Real>(
                std::move(rx), ConvertTo(rx, std::move(by)))};
          },
          [&](BOZLiteralConstant &&bx,
              Expr<SomeReal> &&ry) -> ConvertRealOperandsResult {
            return {AsSameKindExprs<TypeCategory::Real>(
                ConvertTo(ry, std::move(bx)), std::move(ry))};
          },
          [&](auto &&, auto &&) -> ConvertRealOperandsResult {
            messages.Say("operands must be INTEGER or REAL"_err_en_US);
            return std::nullopt;
          },
      },
      std::move(x.u), std::move(y.u));
}

// Helpers for NumericOperation and its subroutines below.
static std::optional<Expr<SomeType>> NoExpr() { return std::nullopt; }

template<TypeCategory CAT>
std::optional<Expr<SomeType>> Package(Expr<SomeKind<CAT>> &&catExpr) {
  return {AsGenericExpr(std::move(catExpr))};
}
template<TypeCategory CAT>
std::optional<Expr<SomeType>> Package(
    std::optional<Expr<SomeKind<CAT>>> &&catExpr) {
  if (catExpr.has_value()) {
    return {AsGenericExpr(std::move(*catExpr))};
  }
  return NoExpr();
}

// Mixed REAL+INTEGER operations.  REAL**INTEGER is a special case that
// does not require conversion of the exponent expression.
template<template<typename> class OPR>
std::optional<Expr<SomeType>> MixedRealLeft(
    Expr<SomeReal> &&rx, Expr<SomeInteger> &&iy) {
  return Package(std::visit(
      [&](auto &&rxk) -> Expr<SomeReal> {
        using resultType = ResultType<decltype(rxk)>;
        if constexpr (std::is_same_v<OPR<resultType>, Power<resultType>>) {
          return AsCategoryExpr(
              RealToIntPower<resultType>{std::move(rxk), std::move(iy)});
        }
        // G++ 8.1.0 emits bogus warnings about missing return statements if
        // this statement is wrapped in an "else", as it should be.
        return AsCategoryExpr(OPR<resultType>{
            std::move(rxk), ConvertToType<resultType>(std::move(iy))});
      },
      std::move(rx.u)));
}

std::optional<Expr<SomeComplex>> ConstructComplex(
    parser::ContextualMessages &messages, Expr<SomeType> &&real,
    Expr<SomeType> &&imaginary, int defaultRealKind) {
  if (auto converted{ConvertRealOperands(
          messages, std::move(real), std::move(imaginary), defaultRealKind)}) {
    return {std::visit(
        [](auto &&pair) {
          return MakeComplex(std::move(pair[0]), std::move(pair[1]));
        },
        std::move(*converted))};
  }
  return std::nullopt;
}

std::optional<Expr<SomeComplex>> ConstructComplex(
    parser::ContextualMessages &messages, std::optional<Expr<SomeType>> &&real,
    std::optional<Expr<SomeType>> &&imaginary, int defaultRealKind) {
  if (auto parts{common::AllPresent(std::move(real), std::move(imaginary))}) {
    return ConstructComplex(messages, std::get<0>(std::move(*parts)),
        std::get<1>(std::move(*parts)), defaultRealKind);
  }
  return std::nullopt;
}

Expr<SomeReal> GetComplexPart(const Expr<SomeComplex> &z, bool isImaginary) {
  return std::visit(
      [&](const auto &zk) {
        static constexpr int kind{ResultType<decltype(zk)>::kind};
        return AsCategoryExpr(ComplexComponent<kind>{isImaginary, zk});
      },
      z.u);
}

// Handle mixed COMPLEX+REAL (or INTEGER) operations in a better way
// than just converting the second operand to COMPLEX and performing the
// corresponding COMPLEX+COMPLEX operation.
template<template<typename> class OPR, TypeCategory RCAT>
std::optional<Expr<SomeType>> MixedComplexLeft(
    parser::ContextualMessages &messages, Expr<SomeComplex> &&zx,
    Expr<SomeKind<RCAT>> &&iry, int defaultRealKind) {
  Expr<SomeReal> zr{GetComplexPart(zx, false)};
  Expr<SomeReal> zi{GetComplexPart(zx, true)};
  if constexpr (std::is_same_v<OPR<LargestReal>, Add<LargestReal>> ||
      std::is_same_v<OPR<LargestReal>, Subtract<LargestReal>>) {
    // (a,b) + x -> (a+x, b)
    // (a,b) - x -> (a-x, b)
    if (std::optional<Expr<SomeType>> rr{
            NumericOperation<OPR>(messages, AsGenericExpr(std::move(zr)),
                AsGenericExpr(std::move(iry)), defaultRealKind)}) {
      return Package(ConstructComplex(messages, std::move(*rr),
          AsGenericExpr(std::move(zi)), defaultRealKind));
    }
  } else if constexpr (std::is_same_v<OPR<LargestReal>,
                           Multiply<LargestReal>> ||
      std::is_same_v<OPR<LargestReal>, Divide<LargestReal>>) {
    // (a,b) * x -> (a*x, b*x)
    // (a,b) / x -> (a/x, b/x)
    auto copy{iry};
    auto rr{NumericOperation<Multiply>(messages, AsGenericExpr(std::move(zr)),
        AsGenericExpr(std::move(iry)), defaultRealKind)};
    auto ri{NumericOperation<Multiply>(messages, AsGenericExpr(std::move(zi)),
        AsGenericExpr(std::move(copy)), defaultRealKind)};
    if (auto parts{common::AllPresent(std::move(rr), std::move(ri))}) {
      return Package(ConstructComplex(messages, std::get<0>(std::move(*parts)),
          std::get<1>(std::move(*parts)), defaultRealKind));
    }
  } else if constexpr (RCAT == TypeCategory::Integer &&
      std::is_same_v<OPR<LargestReal>, Power<LargestReal>>) {
    // COMPLEX**INTEGER is a special case that doesn't convert the exponent.
    static_assert(RCAT == TypeCategory::Integer);
    return Package(std::visit(
        [&](auto &&zxk) {
          using Ty = ResultType<decltype(zxk)>;
          return AsCategoryExpr(
              AsExpr(RealToIntPower<Ty>{std::move(zxk), std::move(iry)}));
        },
        std::move(zx.u)));
  } else if (defaultRealKind != 666) {  // dodge unused parameter warning
    // (a,b) ** x -> (a,b) ** (x,0)
    Expr<SomeComplex> zy{ConvertTo(zx, std::move(iry))};
    return Package(PromoteAndCombine<OPR>(std::move(zx), std::move(zy)));
  }
  return NoExpr();
}

// Mixed COMPLEX operations with the COMPLEX operand on the right.
//  x + (a,b) -> (x+a, b)
//  x - (a,b) -> (x-a, -b)
//  x * (a,b) -> (x*a, x*b)
//  x / (a,b) -> (x,0) / (a,b)   (and **)
template<template<typename> class OPR, TypeCategory LCAT>
std::optional<Expr<SomeType>> MixedComplexRight(
    parser::ContextualMessages &messages, Expr<SomeKind<LCAT>> &&irx,
    Expr<SomeComplex> &&zy, int defaultRealKind) {
  if constexpr (std::is_same_v<OPR<LargestReal>, Add<LargestReal>> ||
      std::is_same_v<OPR<LargestReal>, Multiply<LargestReal>>) {
    // x + (a,b) -> (a,b) + x -> (a+x, b)
    // x * (a,b) -> (a,b) * x -> (a*x, b*x)
    return MixedComplexLeft<Add, LCAT>(
        messages, std::move(zy), std::move(irx), defaultRealKind);
  } else if constexpr (std::is_same_v<OPR<LargestReal>,
                           Subtract<LargestReal>>) {
    // x - (a,b) -> (x-a, -b)
    Expr<SomeReal> zr{GetComplexPart(zy, false)};
    Expr<SomeReal> zi{GetComplexPart(zy, true)};
    if (std::optional<Expr<SomeType>> rr{
            NumericOperation<Subtract>(messages, AsGenericExpr(std::move(irx)),
                AsGenericExpr(std::move(zr)), defaultRealKind)}) {
      return Package(ConstructComplex(messages, std::move(*rr),
          AsGenericExpr(-std::move(zi)), defaultRealKind));
    }
  } else if (defaultRealKind != 666) {  // dodge unused parameter warning
    // x / (a,b) -> (x,0) / (a,b)
    Expr<SomeComplex> zx{ConvertTo(zy, std::move(irx))};
    return Package(PromoteAndCombine<OPR>(std::move(zx), std::move(zy)));
  }
  return NoExpr();
}

// N.B. When a "typeless" BOZ literal constant appears as one (not both!) of
// the operands to a dyadic operation where one is permitted, it assumes the
// type and kind of the other operand.
template<template<typename> class OPR>
std::optional<Expr<SomeType>> NumericOperation(
    parser::ContextualMessages &messages, Expr<SomeType> &&x,
    Expr<SomeType> &&y, int defaultRealKind) {
  return std::visit(
      common::visitors{
          [](Expr<SomeInteger> &&ix, Expr<SomeInteger> &&iy) {
            return Package(PromoteAndCombine<OPR, TypeCategory::Integer>(
                std::move(ix), std::move(iy)));
          },
          [](Expr<SomeReal> &&rx, Expr<SomeReal> &&ry) {
            return Package(PromoteAndCombine<OPR, TypeCategory::Real>(
                std::move(rx), std::move(ry)));
          },
          // Mixed REAL/INTEGER operations
          [](Expr<SomeReal> &&rx, Expr<SomeInteger> &&iy) {
            return MixedRealLeft<OPR>(std::move(rx), std::move(iy));
          },
          [](Expr<SomeInteger> &&ix, Expr<SomeReal> &&ry) {
            return Package(std::visit(
                [&](auto &&ryk) -> Expr<SomeReal> {
                  using resultType = ResultType<decltype(ryk)>;
                  return AsCategoryExpr(
                      OPR<resultType>{ConvertToType<resultType>(std::move(ix)),
                          std::move(ryk)});
                },
                std::move(ry.u)));
          },
          // Homogeneous and mixed COMPLEX operations
          [](Expr<SomeComplex> &&zx, Expr<SomeComplex> &&zy) {
            return Package(PromoteAndCombine<OPR, TypeCategory::Complex>(
                std::move(zx), std::move(zy)));
          },
          [&](Expr<SomeComplex> &&zx, Expr<SomeInteger> &&zy) {
            return MixedComplexLeft<OPR>(
                messages, std::move(zx), std::move(zy), defaultRealKind);
          },
          [&](Expr<SomeComplex> &&zx, Expr<SomeReal> &&zy) {
            return MixedComplexLeft<OPR>(
                messages, std::move(zx), std::move(zy), defaultRealKind);
          },
          [&](Expr<SomeInteger> &&zx, Expr<SomeComplex> &&zy) {
            return MixedComplexRight<OPR>(
                messages, std::move(zx), std::move(zy), defaultRealKind);
          },
          [&](Expr<SomeReal> &&zx, Expr<SomeComplex> &&zy) {
            return MixedComplexRight<OPR>(
                messages, std::move(zx), std::move(zy), defaultRealKind);
          },
          // Operations with one typeless operand
          [&](BOZLiteralConstant &&bx, Expr<SomeInteger> &&iy) {
            return NumericOperation<OPR>(messages,
                AsGenericExpr(ConvertTo(iy, std::move(bx))), std::move(y),
                defaultRealKind);
          },
          [&](BOZLiteralConstant &&bx, Expr<SomeReal> &&ry) {
            return NumericOperation<OPR>(messages,
                AsGenericExpr(ConvertTo(ry, std::move(bx))), std::move(y),
                defaultRealKind);
          },
          [&](Expr<SomeInteger> &&ix, BOZLiteralConstant &&by) {
            return NumericOperation<OPR>(messages, std::move(x),
                AsGenericExpr(ConvertTo(ix, std::move(by))), defaultRealKind);
          },
          [&](Expr<SomeReal> &&rx, BOZLiteralConstant &&by) {
            return NumericOperation<OPR>(messages, std::move(x),
                AsGenericExpr(ConvertTo(rx, std::move(by))), defaultRealKind);
          },
          // Default case
          [&](auto &&, auto &&) {
            // TODO: defined operator
            messages.Say("non-numeric operands to numeric operation"_err_en_US);
            return NoExpr();
          },
      },
      std::move(x.u), std::move(y.u));
}

template std::optional<Expr<SomeType>> NumericOperation<Power>(
    parser::ContextualMessages &, Expr<SomeType> &&, Expr<SomeType> &&,
    int defaultRealKind);
template std::optional<Expr<SomeType>> NumericOperation<Multiply>(
    parser::ContextualMessages &, Expr<SomeType> &&, Expr<SomeType> &&,
    int defaultRealKind);
template std::optional<Expr<SomeType>> NumericOperation<Divide>(
    parser::ContextualMessages &, Expr<SomeType> &&, Expr<SomeType> &&,
    int defaultRealKind);
template std::optional<Expr<SomeType>> NumericOperation<Add>(
    parser::ContextualMessages &, Expr<SomeType> &&, Expr<SomeType> &&,
    int defaultRealKind);
template std::optional<Expr<SomeType>> NumericOperation<Subtract>(
    parser::ContextualMessages &, Expr<SomeType> &&, Expr<SomeType> &&,
    int defaultRealKind);

std::optional<Expr<SomeType>> Negation(
    parser::ContextualMessages &messages, Expr<SomeType> &&x) {
  return std::visit(
      common::visitors{
          [&](BOZLiteralConstant &&) {
            messages.Say("BOZ literal cannot be negated"_err_en_US);
            return NoExpr();
          },
          [&](NullPointer &&) {
            messages.Say("NULL() cannot be negated"_err_en_US);
            return NoExpr();
          },
          [&](ProcedureDesignator &&) {
            messages.Say("Subroutine cannot be negated"_err_en_US);
            return NoExpr();
          },
          [&](ProcedureRef &&) {
            messages.Say("Pointer to subroutine cannot be negated"_err_en_US);
            return NoExpr();
          },
          [&](Expr<SomeInteger> &&x) { return Package(-std::move(x)); },
          [&](Expr<SomeReal> &&x) { return Package(-std::move(x)); },
          [&](Expr<SomeComplex> &&x) { return Package(-std::move(x)); },
          [&](Expr<SomeCharacter> &&) {
            // TODO: defined operator
            messages.Say("CHARACTER cannot be negated"_err_en_US);
            return NoExpr();
          },
          [&](Expr<SomeLogical> &&) {
            // TODO: defined operator
            messages.Say("LOGICAL cannot be negated"_err_en_US);
            return NoExpr();
          },
          [&](Expr<SomeDerived> &&) {
            // TODO: defined operator
            messages.Say("Operand cannot be negated"_err_en_US);
            return NoExpr();
          },
      },
      std::move(x.u));
}

Expr<SomeLogical> LogicalNegation(Expr<SomeLogical> &&x) {
  return std::visit(
      [](auto &&xk) { return AsCategoryExpr(LogicalNegation(std::move(xk))); },
      std::move(x.u));
}

template<typename T>
Expr<LogicalResult> PackageRelation(
    RelationalOperator opr, Expr<T> &&x, Expr<T> &&y) {
  static_assert(IsSpecificIntrinsicType<T>);
  return Expr<LogicalResult>{
      Relational<SomeType>{Relational<T>{opr, std::move(x), std::move(y)}}};
}

template<TypeCategory CAT>
Expr<LogicalResult> PromoteAndRelate(
    RelationalOperator opr, Expr<SomeKind<CAT>> &&x, Expr<SomeKind<CAT>> &&y) {
  return std::visit(
      [=](auto &&xy) {
        return PackageRelation(opr, std::move(xy[0]), std::move(xy[1]));
      },
      AsSameKindExprs(std::move(x), std::move(y)));
}

std::optional<Expr<LogicalResult>> Relate(parser::ContextualMessages &messages,
    RelationalOperator opr, Expr<SomeType> &&x, Expr<SomeType> &&y) {
  return std::visit(
      common::visitors{
          [=](Expr<SomeInteger> &&ix,
              Expr<SomeInteger> &&iy) -> std::optional<Expr<LogicalResult>> {
            return PromoteAndRelate(opr, std::move(ix), std::move(iy));
          },
          [=](Expr<SomeReal> &&rx,
              Expr<SomeReal> &&ry) -> std::optional<Expr<LogicalResult>> {
            return PromoteAndRelate(opr, std::move(rx), std::move(ry));
          },
          [&](Expr<SomeReal> &&rx, Expr<SomeInteger> &&iy) {
            return Relate(messages, opr, std::move(x),
                AsGenericExpr(ConvertTo(rx, std::move(iy))));
          },
          [&](Expr<SomeInteger> &&ix, Expr<SomeReal> &&ry) {
            return Relate(messages, opr,
                AsGenericExpr(ConvertTo(ry, std::move(ix))), std::move(y));
          },
          [&](Expr<SomeComplex> &&zx,
              Expr<SomeComplex> &&zy) -> std::optional<Expr<LogicalResult>> {
            if (opr != RelationalOperator::EQ &&
                opr != RelationalOperator::NE) {
              messages.Say(
                  "COMPLEX data may be compared only for equality"_err_en_US);
            } else {
              auto rr{Relate(messages, opr,
                  AsGenericExpr(GetComplexPart(zx, false)),
                  AsGenericExpr(GetComplexPart(zy, false)))};
              auto ri{
                  Relate(messages, opr, AsGenericExpr(GetComplexPart(zx, true)),
                      AsGenericExpr(GetComplexPart(zy, true)))};
              if (auto parts{
                      common::AllPresent(std::move(rr), std::move(ri))}) {
                // (a,b)==(c,d) -> (a==c) .AND. (b==d)
                // (a,b)/=(c,d) -> (a/=c) .OR. (b/=d)
                LogicalOperator combine{opr == RelationalOperator::EQ
                        ? LogicalOperator::And
                        : LogicalOperator::Or};
                return Expr<LogicalResult>{
                    LogicalOperation<LogicalResult::kind>{combine,
                        std::get<0>(std::move(*parts)),
                        std::get<1>(std::move(*parts))}};
              }
            }
            return std::nullopt;
          },
          [&](Expr<SomeComplex> &&zx, Expr<SomeInteger> &&iy) {
            return Relate(messages, opr, std::move(x),
                AsGenericExpr(ConvertTo(zx, std::move(iy))));
          },
          [&](Expr<SomeComplex> &&zx, Expr<SomeReal> &&ry) {
            return Relate(messages, opr, std::move(x),
                AsGenericExpr(ConvertTo(zx, std::move(ry))));
          },
          [&](Expr<SomeInteger> &&ix, Expr<SomeComplex> &&zy) {
            return Relate(messages, opr,
                AsGenericExpr(ConvertTo(zy, std::move(ix))), std::move(y));
          },
          [&](Expr<SomeReal> &&rx, Expr<SomeComplex> &&zy) {
            return Relate(messages, opr,
                AsGenericExpr(ConvertTo(zy, std::move(rx))), std::move(y));
          },
          [&](Expr<SomeCharacter> &&cx, Expr<SomeCharacter> &&cy) {
            return std::visit(
                [&](auto &&cxk,
                    auto &&cyk) -> std::optional<Expr<LogicalResult>> {
                  using Ty = ResultType<decltype(cxk)>;
                  if constexpr (std::is_same_v<Ty, ResultType<decltype(cyk)>>) {
                    return PackageRelation(opr, std::move(cxk), std::move(cyk));
                  } else {
                    messages.Say(
                        "CHARACTER operands do not have same KIND"_err_en_US);
                    return std::nullopt;
                  }
                },
                std::move(cx.u), std::move(cy.u));
          },
          // Default case
          [&](auto &&, auto &&) {
            // TODO: defined operator
            messages.Say(
                "relational operands do not have comparable types"_err_en_US);
            return std::optional<Expr<LogicalResult>>{};
          },
      },
      std::move(x.u), std::move(y.u));
}

Expr<SomeLogical> BinaryLogicalOperation(
    LogicalOperator opr, Expr<SomeLogical> &&x, Expr<SomeLogical> &&y) {
  return std::visit(
      [=](auto &&xy) {
        using Ty = ResultType<decltype(xy[0])>;
        return Expr<SomeLogical>{BinaryLogicalOperation<Ty::kind>(
            opr, std::move(xy[0]), std::move(xy[1]))};
      },
      AsSameKindExprs(std::move(x), std::move(y)));
}

template<TypeCategory TO>
std::optional<Expr<SomeType>> ConvertToNumeric(int kind, Expr<SomeType> &&x) {
  static_assert(common::IsNumericTypeCategory(TO));
  return std::visit(
      [=](auto &&cx) -> std::optional<Expr<SomeType>> {
        using cxType = std::decay_t<decltype(cx)>;
        if constexpr (!common::HasMember<cxType, TypelessExpression>) {
          if constexpr (IsNumericTypeCategory(ResultType<cxType>::category)) {
            return Expr<SomeType>{ConvertToKind<TO>(kind, std::move(cx))};
          }
        }
        return std::nullopt;
      },
      std::move(x.u));
}

std::optional<Expr<SomeType>> ConvertToType(
    const DynamicType &type, Expr<SomeType> &&x) {
  switch (type.category()) {
  case TypeCategory::Integer:
    if (auto *boz{std::get_if<BOZLiteralConstant>(&x.u)}) {
      // Extension to C7109: allow BOZ literals to appear in integer contexts
      // when the type is unambiguous.
      return Expr<SomeType>{
          ConvertToKind<TypeCategory::Integer>(type.kind(), std::move(*boz))};
    }
    return ConvertToNumeric<TypeCategory::Integer>(type.kind(), std::move(x));
  case TypeCategory::Real:
    if (auto *boz{std::get_if<BOZLiteralConstant>(&x.u)}) {
      return Expr<SomeType>{
          ConvertToKind<TypeCategory::Real>(type.kind(), std::move(*boz))};
    }
    return ConvertToNumeric<TypeCategory::Real>(type.kind(), std::move(x));
  case TypeCategory::Complex:
    return ConvertToNumeric<TypeCategory::Complex>(type.kind(), std::move(x));
  case TypeCategory::Character:
    if (auto *cx{UnwrapExpr<Expr<SomeCharacter>>(x)}) {
      auto converted{
          ConvertToKind<TypeCategory::Character>(type.kind(), std::move(*cx))};
      if (type.charLength() != nullptr) {
        if (const auto &len{type.charLength()->GetExplicit()}) {
          Expr<SomeInteger> lenParam{*len};
          Expr<SubscriptInteger> length{Convert<SubscriptInteger>{lenParam}};
          converted = std::visit(
              [&](auto &&x) {
                using Ty = std::decay_t<decltype(x)>;
                using CharacterType = typename Ty::Result;
                return Expr<SomeCharacter>{
                    Expr<CharacterType>{SetLength<CharacterType::kind>{
                        std::move(x), std::move(length)}}};
              },
              std::move(converted.u));
        }
      }
      return Expr<SomeType>{std::move(converted)};
    }
    break;
  case TypeCategory::Logical:
    if (auto *cx{UnwrapExpr<Expr<SomeLogical>>(x)}) {
      return Expr<SomeType>{
          ConvertToKind<TypeCategory::Logical>(type.kind(), std::move(*cx))};
    }
    break;
  case TypeCategory::Derived:
    if (auto fromType{x.GetType()}) {
      if (type == *fromType) {
        return std::move(x);
      }
    }
    break;
  default: CRASH_NO_CASE;
  }
  return std::nullopt;
}

std::optional<Expr<SomeType>> ConvertToType(
    const DynamicType &to, std::optional<Expr<SomeType>> &&x) {
  if (x.has_value()) {
    return ConvertToType(to, std::move(*x));
  } else {
    return std::nullopt;
  }
}

std::optional<Expr<SomeType>> ConvertToType(
    const semantics::Symbol &symbol, Expr<SomeType> &&x) {
  if (int xRank{x.Rank()}; xRank > 0) {
    if (symbol.Rank() != xRank) {
      return std::nullopt;
    }
  }
  if (auto symType{DynamicType::From(symbol)}) {
    return ConvertToType(*symType, std::move(x));
  }
  return std::nullopt;
}

std::optional<Expr<SomeType>> ConvertToType(
    const semantics::Symbol &to, std::optional<Expr<SomeType>> &&x) {
  if (x.has_value()) {
    return ConvertToType(to, std::move(*x));
  } else {
    return std::nullopt;
  }
}

bool IsAssumedRank(const semantics::Symbol &symbol0) {
  const semantics::Symbol &symbol{ResolveAssociations(symbol0)};
  if (const auto *details{symbol.detailsIf<semantics::ObjectEntityDetails>()}) {
    return details->IsAssumedRank();
  } else {
    return false;
  }
}

bool IsAssumedRank(const ActualArgument &arg) {
  if (const auto *expr{arg.UnwrapExpr()}) {
    return IsAssumedRank(*expr);
  } else {
    const semantics::Symbol *assumedTypeDummy{arg.GetAssumedTypeDummy()};
    CHECK(assumedTypeDummy != nullptr);
    return IsAssumedRank(*assumedTypeDummy);
  }
}

// GetLastTarget()
GetLastTargetVisitor::GetLastTargetVisitor(std::nullptr_t) {}
void GetLastTargetVisitor::Handle(const semantics::Symbol &x) {
  if (x.attrs().HasAny({semantics::Attr::POINTER, semantics::Attr::TARGET})) {
    Return(&x);
  } else {
    Return(nullptr);
  }
}
void GetLastTargetVisitor::Pre(const Component &x) {
  const semantics::Symbol &symbol{x.GetLastSymbol()};
  if (symbol.attrs().HasAny(
          {semantics::Attr::POINTER, semantics::Attr::TARGET})) {
    Return(&symbol);
  } else if (symbol.attrs().test(semantics::Attr::ALLOCATABLE)) {
    Return(nullptr);
  }
}

const semantics::Symbol &ResolveAssociations(const semantics::Symbol &symbol) {
  if (const auto *details{symbol.detailsIf<semantics::AssocEntityDetails>()}) {
    if (const Symbol * nested{UnwrapWholeSymbolDataRef(details->expr())}) {
      return ResolveAssociations(*nested);
    }
  }
  return symbol;
}

}
