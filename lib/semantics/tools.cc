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

#include "tools.h"
#include "scope.h"
#include "semantics.h"
#include "symbol.h"
#include "type.h"
#include "../common/Fortran.h"
#include "../common/indirection.h"
#include "../parser/message.h"
#include "../parser/parse-tree.h"
#include <algorithm>
#include <set>
#include <variant>

namespace Fortran::semantics {

static const Symbol *FindCommonBlockInScope(
    const Scope &scope, const Symbol &object) {
  for (const auto &pair : scope.commonBlocks()) {
    const Symbol &block{*pair.second};
    if (IsCommonBlockContaining(block, object)) {
      return &block;
    }
  }
  return nullptr;
}

const Symbol *FindCommonBlockContaining(const Symbol &object) {
  for (const Scope *scope{&object.owner()}; !scope->IsGlobal();
       scope = &scope->parent()) {
    if (const Symbol * block{FindCommonBlockInScope(*scope, object)}) {
      return block;
    }
  }
  return nullptr;
}

const Scope *FindProgramUnitContaining(const Scope &start) {
  const Scope *scope{&start};
  while (scope != nullptr) {
    switch (scope->kind()) {
    case Scope::Kind::Module:
    case Scope::Kind::MainProgram:
    case Scope::Kind::Subprogram: return scope;
    case Scope::Kind::Global: return nullptr;
    case Scope::Kind::DerivedType:
    case Scope::Kind::Block:
    case Scope::Kind::Forall:
    case Scope::Kind::ImpliedDos: scope = &scope->parent();
    }
  }
  return nullptr;
}

const Scope *FindProgramUnitContaining(const Symbol &symbol) {
  return FindProgramUnitContaining(symbol.owner());
}

const Scope *FindPureProcedureContaining(const Scope *scope) {
  scope = FindProgramUnitContaining(*scope);
  while (scope != nullptr) {
    if (IsPureProcedure(*scope)) {
      return scope;
    }
    scope = FindProgramUnitContaining(scope->parent());
  }
  return nullptr;
}

bool IsCommonBlockContaining(const Symbol &block, const Symbol &object) {
  const auto &objects{block.get<CommonBlockDetails>().objects()};
  auto found{std::find(objects.begin(), objects.end(), &object)};
  return found != objects.end();
}

bool IsUseAssociated(const Symbol &symbol, const Scope &scope) {
  const Scope *owner{FindProgramUnitContaining(symbol.GetUltimate().owner())};
  return owner != nullptr && owner->kind() == Scope::Kind::Module &&
      owner != FindProgramUnitContaining(scope);
}

bool DoesScopeContain(
    const Scope *maybeAncestor, const Scope &maybeDescendent) {
  if (maybeAncestor != nullptr) {
    const Scope *scope{&maybeDescendent};
    while (!scope->IsGlobal()) {
      scope = &scope->parent();
      if (scope == maybeAncestor) {
        return true;
      }
    }
  }
  return false;
}

bool DoesScopeContain(const Scope *maybeAncestor, const Symbol &symbol) {
  return DoesScopeContain(maybeAncestor, symbol.owner());
}

bool IsHostAssociated(const Symbol &symbol, const Scope &scope) {
  return DoesScopeContain(FindProgramUnitContaining(symbol), scope);
}

bool IsDummy(const Symbol &symbol) {
  if (const auto *details{symbol.detailsIf<ObjectEntityDetails>()}) {
    return details->isDummy();
  } else if (const auto *details{symbol.detailsIf<ProcEntityDetails>()}) {
    return details->isDummy();
  } else {
    return false;
  }
}

bool IsPointerDummy(const Symbol &symbol) {
  return IsPointer(symbol) && IsDummy(symbol);
}

// variable-name
bool IsVariableName(const Symbol &symbol) {
  if (const Symbol * root{GetAssociationRoot(symbol)}) {
    return root->has<ObjectEntityDetails>() && !IsParameter(*root);
  } else {
    return false;
  }
}

// proc-name
bool IsProcName(const Symbol &symbol) {
  return symbol.GetUltimate().has<ProcEntityDetails>();
}

bool IsFunction(const Symbol &symbol) {
  return std::visit(
      common::visitors{
          [](const SubprogramDetails &x) { return x.isFunction(); },
          [&](const SubprogramNameDetails &) {
            return symbol.test(Symbol::Flag::Function);
          },
          [](const ProcEntityDetails &x) {
            const auto &ifc{x.interface()};
            return ifc.type() || (ifc.symbol() && IsFunction(*ifc.symbol()));
          },
          [](const ProcBindingDetails &x) { return IsFunction(x.symbol()); },
          [](const UseDetails &x) { return IsFunction(x.symbol()); },
          [](const auto &) { return false; },
      },
      symbol.details());
}

bool IsPureProcedure(const Symbol &symbol) {
  return symbol.attrs().test(Attr::PURE) && IsProcedure(symbol);
}

bool IsPureProcedure(const Scope &scope) {
  if (const Symbol * symbol{scope.GetSymbol()}) {
    return IsPureProcedure(*symbol);
  } else {
    return false;
  }
}

bool IsProcedure(const Symbol &symbol) {
  return std::visit(
      common::visitors{
          [](const SubprogramDetails &) { return true; },
          [](const SubprogramNameDetails &) { return true; },
          [](const ProcEntityDetails &) { return true; },
          [](const GenericDetails &) { return true; },
          [](const ProcBindingDetails &) { return true; },
          [](const UseDetails &x) { return IsProcedure(x.symbol()); },
          [](const auto &) { return false; },
      },
      symbol.details());
}

bool IsProcedurePointer(const Symbol &symbol) {
  return symbol.has<ProcEntityDetails>() && IsPointer(symbol);
}

static const Symbol *FindPointerComponent(
    const Scope &scope, std::set<const Scope *> &visited) {
  if (!scope.IsDerivedType()) {
    return nullptr;
  }
  if (!visited.insert(&scope).second) {
    return nullptr;
  }
  // If there's a top-level pointer component, return it for clearer error
  // messaging.
  for (const auto &pair : scope) {
    const Symbol &symbol{*pair.second};
    if (IsPointer(symbol)) {
      return &symbol;
    }
  }
  for (const auto &pair : scope) {
    const Symbol &symbol{*pair.second};
    if (const auto *details{symbol.detailsIf<ObjectEntityDetails>()}) {
      if (const DeclTypeSpec * type{details->type()}) {
        if (const DerivedTypeSpec * derived{type->AsDerived()}) {
          if (const Scope * nested{derived->scope()}) {
            if (const Symbol *
                pointer{FindPointerComponent(*nested, visited)}) {
              return pointer;
            }
          }
        }
      }
    }
  }
  return nullptr;
}

const Symbol *FindPointerComponent(const Scope &scope) {
  std::set<const Scope *> visited;
  return FindPointerComponent(scope, visited);
}

const Symbol *FindPointerComponent(const DerivedTypeSpec &derived) {
  if (const Scope * scope{derived.scope()}) {
    return FindPointerComponent(*scope);
  } else {
    return nullptr;
  }
}

const Symbol *FindPointerComponent(const DeclTypeSpec &type) {
  if (const DerivedTypeSpec * derived{type.AsDerived()}) {
    return FindPointerComponent(*derived);
  } else {
    return nullptr;
  }
}

const Symbol *FindPointerComponent(const DeclTypeSpec *type) {
  return type ? FindPointerComponent(*type) : nullptr;
}

const Symbol *FindPointerComponent(const Symbol &symbol) {
  return IsPointer(symbol) ? &symbol : FindPointerComponent(symbol.GetType());
}

// C1594 specifies several ways by which an object might be globally visible.
const Symbol *FindExternallyVisibleObject(
    const Symbol &object, const Scope &scope) {
  // TODO: Storage association with any object for which this predicate holds,
  // once EQUIVALENCE is supported.
  if (IsUseAssociated(object, scope) || IsHostAssociated(object, scope) ||
      (IsPureProcedure(scope) && IsPointerDummy(object)) ||
      (IsIntentIn(object) && IsDummy(object))) {
    return &object;
  } else if (const Symbol * block{FindCommonBlockContaining(object)}) {
    return block;
  } else {
    return nullptr;
  }
}

bool ExprHasTypeCategory(
    const SomeExpr &expr, const common::TypeCategory &type) {
  auto dynamicType{expr.GetType()};
  return dynamicType.has_value() && dynamicType->category() == type;
}

bool ExprTypeKindIsDefault(
    const SomeExpr &expr, const SemanticsContext &context) {
  auto dynamicType{expr.GetType()};
  return dynamicType.has_value() &&
      dynamicType->category() != common::TypeCategory::Derived &&
      dynamicType->kind() == context.GetDefaultKind(dynamicType->category());
}

const Symbol *FindInterface(const Symbol &symbol) {
  return std::visit(
      common::visitors{
          [](const ProcEntityDetails &details) {
            return details.interface().symbol();
          },
          [](const ProcBindingDetails &details) { return &details.symbol(); },
          [](const auto &) -> const Symbol * { return nullptr; },
      },
      symbol.details());
}

const Symbol *FindSubprogram(const Symbol &symbol) {
  return std::visit(
      common::visitors{
          [&](const ProcEntityDetails &details) -> const Symbol * {
            if (const Symbol * interface{details.interface().symbol()}) {
              return FindSubprogram(*interface);
            } else {
              return &symbol;
            }
          },
          [](const ProcBindingDetails &details) {
            return FindSubprogram(details.symbol());
          },
          [&](const SubprogramDetails &) { return &symbol; },
          [](const UseDetails &details) {
            return FindSubprogram(details.symbol());
          },
          [](const HostAssocDetails &details) {
            return FindSubprogram(details.symbol());
          },
          [](const auto &) -> const Symbol * { return nullptr; },
      },
      symbol.details());
}

const Symbol *FindFunctionResult(const Symbol &symbol) {
  if (const Symbol * subp{FindSubprogram(symbol)}) {
    if (const auto &subpDetails{subp->detailsIf<SubprogramDetails>()}) {
      if (subpDetails->isFunction()) {
        return &subpDetails->result();
      }
    }
  }
  return nullptr;
}

static const Symbol *GetAssociatedVariable(const AssocEntityDetails &details) {
  if (const MaybeExpr & expr{details.expr()}) {
    if (evaluate::IsVariable(*expr)) {
      if (const Symbol * varSymbol{evaluate::GetLastSymbol(*expr)}) {
        return GetAssociationRoot(*varSymbol);
      }
    }
  }
  return nullptr;
}

// Return the Symbol of the variable of a construct association, if it exists
// Return nullptr if the name is associated with an expression
const Symbol *GetAssociationRoot(const Symbol &symbol) {
  const Symbol &ultimate{symbol.GetUltimate()};
  if (const auto *details{ultimate.detailsIf<AssocEntityDetails>()}) {
    // We have a construct association
    return GetAssociatedVariable(*details);
  } else {
    return &ultimate;
  }
}

bool IsExtensibleType(const DerivedTypeSpec *derived) {
  return derived && !IsIsoCType(derived) &&
      !derived->typeSymbol().attrs().test(Attr::BIND_C) &&
      !derived->typeSymbol().get<DerivedTypeDetails>().sequence();
}

bool IsDerivedTypeFromModule(
    const DerivedTypeSpec *derived, const char *module, const char *name) {
  if (!derived) {
    return false;
  } else {
    const auto &symbol{derived->typeSymbol()};
    return symbol.name() == name && symbol.owner().IsModule() &&
        symbol.owner().name() == module;
  }
}

bool IsIsoCType(const DerivedTypeSpec *derived) {
  return IsDerivedTypeFromModule(derived, "iso_c_binding", "c_ptr") ||
      IsDerivedTypeFromModule(derived, "iso_c_binding", "c_funptr");
}

bool IsTeamType(const DerivedTypeSpec *derived) {
  return IsDerivedTypeFromModule(derived, "iso_fortran_env", "team_type");
}

bool IsEventTypeOrLockType(const DerivedTypeSpec *derivedTypeSpec) {
  return IsDerivedTypeFromModule(
             derivedTypeSpec, "iso_fortran_env", "event_type") ||
      IsDerivedTypeFromModule(derivedTypeSpec, "iso_fortran_env", "lock_type");
}

bool IsOrContainsEventOrLockComponent(const Symbol &symbol) {
  if (const Symbol * root{GetAssociationRoot(symbol)}) {
    if (const auto *details{root->detailsIf<ObjectEntityDetails>()}) {
      if (const DeclTypeSpec * type{details->type()}) {
        if (const DerivedTypeSpec * derived{type->AsDerived()}) {
          return IsEventTypeOrLockType(derived) ||
              FindEventOrLockPotentialComponent(*derived);
        }
      }
    }
  }
  return false;
}

bool IsSaved(const Symbol &symbol) {
  auto scopeKind{symbol.owner().kind()};
  if (scopeKind == Scope::Kind::MainProgram ||
      scopeKind == Scope::Kind::Module) {
    return true;
  } else if (scopeKind == Scope::Kind::DerivedType) {
    return false;  // this is a component
  } else if (symbol.attrs().test(Attr::SAVE)) {
    return true;
  } else if (const auto *object{symbol.detailsIf<ObjectEntityDetails>()}) {
    return object->init().has_value();
  } else if (IsProcedurePointer(symbol)) {
    return symbol.get<ProcEntityDetails>().init().has_value();
  } else {
    return false;
  }
}

// Check this symbol suitable as a type-bound procedure - C769
bool CanBeTypeBoundProc(const Symbol *symbol) {
  if (symbol == nullptr || IsDummy(*symbol) || IsProcedurePointer(*symbol)) {
    return false;
  } else if (symbol->has<SubprogramNameDetails>()) {
    return symbol->owner().kind() == Scope::Kind::Module;
  } else if (auto *details{symbol->detailsIf<SubprogramDetails>()}) {
    return symbol->owner().kind() == Scope::Kind::Module ||
        details->isInterface();
  } else if (const auto *proc{symbol->detailsIf<ProcEntityDetails>()}) {
    return !symbol->attrs().test(Attr::INTRINSIC) &&
        proc->HasExplicitInterface();
  } else {
    return false;
  }
}

bool IsFinalizable(const Symbol &symbol) {
  if (const DeclTypeSpec * type{symbol.GetType()}) {
    if (const DerivedTypeSpec * derived{type->AsDerived()}) {
      if (const Scope * scope{derived->scope()}) {
        for (auto &pair : *scope) {
          Symbol &symbol{*pair.second};
          if (symbol.has<FinalProcDetails>()) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

bool IsCoarray(const Symbol &symbol) { return symbol.Corank() > 0; }

bool IsExternalInPureContext(const Symbol &symbol, const Scope &scope) {
  if (const auto *pureProc{semantics::FindPureProcedureContaining(&scope)}) {
    if (const Symbol * root{GetAssociationRoot(symbol)}) {
      if (FindExternallyVisibleObject(*root, *pureProc)) {
        return true;
      }
    }
  }
  return false;
}

bool InProtectedContext(const Symbol &symbol, const Scope &currentScope) {
  return IsProtected(symbol) && !IsHostAssociated(symbol, currentScope);
}

// C1101 and C1158
// TODO Need to check for the case of a variable that has a vector subscript
// that is construct associated, also need to check for a coindexed object
std::optional<parser::MessageFixedText> WhyNotModifiable(
    const Symbol &symbol, const Scope &scope) {
  const Symbol *root{GetAssociationRoot(symbol)};
  if (!root) {
    return "'%s' is construct associated with an expression"_en_US;
  } else if (InProtectedContext(*root, scope)) {
    return "'%s' is protected in this scope"_en_US;
  } else if (IsExternalInPureContext(*root, scope)) {
    return "'%s' is externally visible and referenced in a PURE"
           " procedure"_en_US;
  } else if (IsOrContainsEventOrLockComponent(*root)) {
    return "'%s' is an entity with either an EVENT_TYPE or LOCK_TYPE"_en_US;
  } else if (IsIntentIn(*root)) {
    return "'%s' is an INTENT(IN) dummy argument"_en_US;
  } else if (!IsVariableName(*root)) {
    return "'%s' is not a variable"_en_US;
  } else {
    return std::nullopt;
  }
}

static const DeclTypeSpec &InstantiateIntrinsicType(Scope &scope,
    const DeclTypeSpec &spec, SemanticsContext &semanticsContext) {
  const IntrinsicTypeSpec *intrinsic{spec.AsIntrinsic()};
  CHECK(intrinsic != nullptr);
  if (evaluate::ToInt64(intrinsic->kind()).has_value()) {
    return spec;  // KIND is already a known constant
  }
  // The expression was not originally constant, but now it must be so
  // in the context of a parameterized derived type instantiation.
  KindExpr copy{intrinsic->kind()};
  evaluate::FoldingContext &foldingContext{semanticsContext.foldingContext()};
  copy = evaluate::Fold(foldingContext, std::move(copy));
  int kind{semanticsContext.GetDefaultKind(intrinsic->category())};
  if (auto value{evaluate::ToInt64(copy)}) {
    if (evaluate::IsValidKindOfIntrinsicType(intrinsic->category(), *value)) {
      kind = *value;
    } else {
      foldingContext.messages().Say(
          "KIND parameter value (%jd) of intrinsic type %s "
          "did not resolve to a supported value"_err_en_US,
          static_cast<std::intmax_t>(*value),
          parser::ToUpperCaseLetters(
              common::EnumToString(intrinsic->category())));
    }
  }
  switch (spec.category()) {
  case DeclTypeSpec::Numeric:
    return scope.MakeNumericType(intrinsic->category(), KindExpr{kind});
  case DeclTypeSpec::Logical:  //
    return scope.MakeLogicalType(KindExpr{kind});
  case DeclTypeSpec::Character:
    return scope.MakeCharacterType(
        ParamValue{spec.characterTypeSpec().length()}, KindExpr{kind});
  default: CRASH_NO_CASE;
  }
}

static const DeclTypeSpec *FindInstantiatedDerivedType(const Scope &scope,
    const DerivedTypeSpec &spec, DeclTypeSpec::Category category) {
  DeclTypeSpec type{category, spec};
  if (const auto *found{scope.FindType(type)}) {
    return found;
  } else if (scope.IsGlobal()) {
    return nullptr;
  } else {
    return FindInstantiatedDerivedType(scope.parent(), spec, category);
  }
}

static Symbol &InstantiateSymbol(const Symbol &, Scope &, SemanticsContext &);

std::list<SourceName> OrderParameterNames(const Symbol &typeSymbol) {
  std::list<SourceName> result;
  if (const DerivedTypeSpec * spec{typeSymbol.GetParentTypeSpec()}) {
    result = OrderParameterNames(spec->typeSymbol());
  }
  const auto &paramNames{typeSymbol.get<DerivedTypeDetails>().paramNames()};
  result.insert(result.end(), paramNames.begin(), paramNames.end());
  return result;
}

SymbolVector OrderParameterDeclarations(const Symbol &typeSymbol) {
  SymbolVector result;
  if (const DerivedTypeSpec * spec{typeSymbol.GetParentTypeSpec()}) {
    result = OrderParameterDeclarations(spec->typeSymbol());
  }
  const auto &paramDecls{typeSymbol.get<DerivedTypeDetails>().paramDecls()};
  result.insert(result.end(), paramDecls.begin(), paramDecls.end());
  return result;
}

void InstantiateDerivedType(DerivedTypeSpec &spec, Scope &containingScope,
    SemanticsContext &semanticsContext) {
  Scope &newScope{containingScope.MakeScope(Scope::Kind::DerivedType)};
  newScope.set_derivedTypeSpec(spec);
  spec.ReplaceScope(newScope);
  const Symbol &typeSymbol{spec.typeSymbol()};
  const Scope *typeScope{typeSymbol.scope()};
  CHECK(typeScope != nullptr);
  for (const Symbol *symbol : OrderParameterDeclarations(typeSymbol)) {
    const SourceName &name{symbol->name()};
    if (typeScope->find(symbol->name()) != typeScope->end()) {
      // This type parameter belongs to the derived type itself, not to
      // one of its parents.  Put the type parameter expression value
      // into the new scope as the initialization value for the parameter.
      if (ParamValue * paramValue{spec.FindParameter(name)}) {
        const TypeParamDetails &details{symbol->get<TypeParamDetails>()};
        paramValue->set_attr(details.attr());
        if (MaybeIntExpr expr{paramValue->GetExplicit()}) {
          // Ensure that any kind type parameters with values are
          // constant by now.
          if (details.attr() == common::TypeParamAttr::Kind) {
            // Any errors in rank and type will have already elicited
            // messages, so don't pile on by complaining further here.
            if (auto maybeDynamicType{expr->GetType()}) {
              if (expr->Rank() == 0 &&
                  maybeDynamicType->category() == TypeCategory::Integer) {
                if (!evaluate::ToInt64(*expr).has_value()) {
                  std::stringstream fortran;
                  fortran << *expr;
                  if (auto *msg{
                          semanticsContext.foldingContext().messages().Say(
                              "Value of kind type parameter '%s' (%s) is not "
                              "a scalar INTEGER constant"_err_en_US,
                              name, fortran.str())}) {
                    msg->Attach(name, "declared here"_en_US);
                  }
                }
              }
            }
          }
          TypeParamDetails instanceDetails{details.attr()};
          if (const DeclTypeSpec * type{details.type()}) {
            instanceDetails.set_type(*type);
          }
          instanceDetails.set_init(std::move(*expr));
          Symbol *parameter{
              newScope.try_emplace(name, std::move(instanceDetails))
                  .first->second};
          CHECK(parameter != nullptr);
        }
      }
    }
  }
  // Instantiate every non-parameter symbol from the original derived
  // type's scope into the new instance.
  auto restorer{semanticsContext.foldingContext().WithPDTInstance(spec)};
  newScope.AddSourceRange(typeScope->sourceRange());
  for (const auto &pair : *typeScope) {
    const Symbol &symbol{*pair.second};
    InstantiateSymbol(symbol, newScope, semanticsContext);
  }
}

void ProcessParameterExpressions(
    DerivedTypeSpec &spec, evaluate::FoldingContext &foldingContext) {
  auto paramDecls{OrderParameterDeclarations(spec.typeSymbol())};
  // Fold the explicit type parameter value expressions first.  Do not
  // fold them within the scope of the derived type being instantiated;
  // these expressions cannot use its type parameters.  Convert the values
  // of the expressions to the declared types of the type parameters.
  for (const Symbol *symbol : paramDecls) {
    const SourceName &name{symbol->name()};
    if (ParamValue * paramValue{spec.FindParameter(name)}) {
      if (const MaybeIntExpr & expr{paramValue->GetExplicit()}) {
        if (auto converted{evaluate::ConvertToType(*symbol, SomeExpr{*expr})}) {
          SomeExpr folded{
              evaluate::Fold(foldingContext, std::move(*converted))};
          if (auto *intExpr{std::get_if<SomeIntExpr>(&folded.u)}) {
            paramValue->SetExplicit(std::move(*intExpr));
            continue;
          }
        }
        std::stringstream fortran;
        fortran << *expr;
        if (auto *msg{foldingContext.messages().Say(
                "Value of type parameter '%s' (%s) is not "
                "convertible to its type"_err_en_US,
                name, fortran.str())}) {
          msg->Attach(name, "declared here"_en_US);
        }
      }
    }
  }
  // Type parameter default value expressions are folded in declaration order
  // within the scope of the derived type so that the values of earlier type
  // parameters are available for use in the default initialization
  // expressions of later parameters.
  auto restorer{foldingContext.WithPDTInstance(spec)};
  for (const Symbol *symbol : paramDecls) {
    const SourceName &name{symbol->name()};
    const TypeParamDetails &details{symbol->get<TypeParamDetails>()};
    MaybeIntExpr expr;
    ParamValue *paramValue{spec.FindParameter(name)};
    if (paramValue == nullptr) {
      expr = evaluate::Fold(foldingContext, common::Clone(details.init()));
    } else if (paramValue->isExplicit()) {
      expr = paramValue->GetExplicit();
    }
    if (expr.has_value()) {
      if (paramValue != nullptr) {
        paramValue->SetExplicit(std::move(*expr));
      } else {
        spec.AddParamValue(
            symbol->name(), ParamValue{std::move(*expr), details.attr()});
      }
    }
  }
}

const DeclTypeSpec &FindOrInstantiateDerivedType(Scope &scope,
    DerivedTypeSpec &&spec, SemanticsContext &semanticsContext,
    DeclTypeSpec::Category category) {
  ProcessParameterExpressions(spec, semanticsContext.foldingContext());
  if (const DeclTypeSpec *
      type{FindInstantiatedDerivedType(scope, spec, category)}) {
    return *type;
  }
  // Create a new instantiation of this parameterized derived type
  // for this particular distinct set of actual parameter values.
  DeclTypeSpec &type{scope.MakeDerivedType(std::move(spec), category)};
  InstantiateDerivedType(type.derivedTypeSpec(), scope, semanticsContext);
  return type;
}

// Clone a Symbol in the context of a parameterized derived type instance
static Symbol &InstantiateSymbol(
    const Symbol &symbol, Scope &scope, SemanticsContext &semanticsContext) {
  evaluate::FoldingContext foldingContext{semanticsContext.foldingContext()};
  CHECK(foldingContext.pdtInstance() != nullptr);
  const DerivedTypeSpec &instanceSpec{*foldingContext.pdtInstance()};
  auto pair{scope.try_emplace(symbol.name(), symbol.attrs())};
  Symbol &result{*pair.first->second};
  if (!pair.second) {
    // Symbol was already present in the scope, which can only happen
    // in the case of type parameters.
    CHECK(symbol.has<TypeParamDetails>());
    return result;
  }
  result.attrs() = symbol.attrs();
  result.flags() = symbol.flags();
  result.set_details(common::Clone(symbol.details()));
  if (auto *details{result.detailsIf<ObjectEntityDetails>()}) {
    if (DeclTypeSpec * origType{result.GetType()}) {
      if (const DerivedTypeSpec * derived{origType->AsDerived()}) {
        DerivedTypeSpec newSpec{*derived};
        if (symbol.test(Symbol::Flag::ParentComp)) {
          // Forward any explicit type parameter values from the
          // derived type spec under instantiation to its parent
          // component derived type spec that define type parameters
          // of the parent component.
          for (const auto &pair : instanceSpec.parameters()) {
            if (scope.find(pair.first) == scope.end()) {
              newSpec.AddParamValue(pair.first, ParamValue{pair.second});
            }
          }
        }
        details->ReplaceType(FindOrInstantiateDerivedType(
            scope, std::move(newSpec), semanticsContext, origType->category()));
      } else if (origType->AsIntrinsic() != nullptr) {
        details->ReplaceType(
            InstantiateIntrinsicType(scope, *origType, semanticsContext));
      } else if (origType->category() != DeclTypeSpec::ClassStar) {
        DIE("instantiated component has type that is "
            "neither intrinsic, derived, nor CLASS(*)");
      }
    }
    details->set_init(
        evaluate::Fold(foldingContext, std::move(details->init())));
    for (ShapeSpec &dim : details->shape()) {
      if (dim.lbound().isExplicit()) {
        dim.lbound().SetExplicit(
            Fold(foldingContext, std::move(dim.lbound().GetExplicit())));
      }
      if (dim.ubound().isExplicit()) {
        dim.ubound().SetExplicit(
            Fold(foldingContext, std::move(dim.ubound().GetExplicit())));
      }
    }
    for (ShapeSpec &dim : details->coshape()) {
      if (dim.lbound().isExplicit()) {
        dim.lbound().SetExplicit(
            Fold(foldingContext, std::move(dim.lbound().GetExplicit())));
      }
      if (dim.ubound().isExplicit()) {
        dim.ubound().SetExplicit(
            Fold(foldingContext, std::move(dim.ubound().GetExplicit())));
      }
    }
  }
  return result;
}

// ComponentIterator implementation

template<ComponentKind componentKind>
typename ComponentIterator<componentKind>::const_iterator
ComponentIterator<componentKind>::const_iterator::Create(
    const DerivedTypeSpec &derived) {
  const_iterator it{};
  const std::list<SourceName> &names{
      derived.typeSymbol().get<DerivedTypeDetails>().componentNames()};
  if (names.empty()) {
    return it;  // end iterator
  } else {
    it.componentPath_.emplace_back(
        ComponentPathNode{nullptr, &derived, names.cbegin()});
    it.Increment();  // search first relevant component (may be the end)
    return it;
  }
}

template<ComponentKind componentKind>
bool ComponentIterator<componentKind>::const_iterator::PlanComponentTraversal(
    const Symbol &component) {
  // only data component can be traversed
  if (const auto *details{component.detailsIf<ObjectEntityDetails>()}) {
    const DeclTypeSpec *type{details->type()};
    if (!type) {
      return false;  // error recovery
    } else if (const auto *derived{type->AsDerived()}) {
      bool traverse{false};
      if constexpr (componentKind == ComponentKind::Ordered) {
        // Order Component (only visit parents)
        traverse = component.test(Symbol::Flag::ParentComp);
      } else if constexpr (componentKind == ComponentKind::Direct) {
        traverse = !IsAllocatableOrPointer(component);
      } else if constexpr (componentKind == ComponentKind::Ultimate) {
        traverse = !IsAllocatableOrPointer(component);
      } else if constexpr (componentKind == ComponentKind::Potential) {
        traverse = !IsPointer(component);
      }
      if (traverse) {
        const Symbol *newTypeSymbol{&derived->typeSymbol()};
        // Avoid infinite loop if the type is already part of the types
        // being visited. It is possible to have "loops in type" because
        // C744 does not forbid to use not yet declared type for
        // ALLOCATABLE or POINTER components.
        for (const auto &node : componentPath_) {
          if (newTypeSymbol == &GetTypeSymbol(node)) {
            return false;
          }
        }
        componentPath_.emplace_back(ComponentPathNode{nullptr, derived,
            newTypeSymbol->get<DerivedTypeDetails>()
                .componentNames()
                .cbegin()});
        return true;
      }
    }  // intrinsic & unlimited polymorphic not traversable
  }
  return false;
}

template<ComponentKind componentKind>
static bool StopAtComponentPre(const Symbol &component) {
  if constexpr (componentKind == ComponentKind::Ordered) {
    // Parent components need to be iterated upon after their
    // sub-components in structure constructor analysis.
    return !component.test(Symbol::Flag::ParentComp);
  } else if constexpr (componentKind == ComponentKind::Direct) {
    return true;
  } else if constexpr (componentKind == ComponentKind::Ultimate) {
    return component.has<ProcEntityDetails>() ||
        IsAllocatableOrPointer(component) ||
        (component.get<ObjectEntityDetails>().type() &&
            component.get<ObjectEntityDetails>().type()->AsIntrinsic());
  } else if constexpr (componentKind == ComponentKind::Potential) {
    return !IsPointer(component);
  }
}

template<ComponentKind componentKind>
static bool StopAtComponentPost(const Symbol &component) {
  if constexpr (componentKind == ComponentKind::Ordered) {
    return component.test(Symbol::Flag::ParentComp);
  } else {
    return false;
  }
}

enum class ComponentVisitState { Resume, Pre, Post };

template<ComponentKind componentKind>
void ComponentIterator<componentKind>::const_iterator::Increment() {
  std::int64_t level{static_cast<std::int64_t>(componentPath_.size()) - 1};
  // Need to know if this is the first incrementation or if the visit is resumed
  // after a user increment.
  ComponentVisitState state{
      level >= 0 && GetComponentSymbol(componentPath_[level])
          ? ComponentVisitState::Resume
          : ComponentVisitState::Pre};
  while (level >= 0) {
    bool descend{false};
    const Scope &scope{DEREF(GetScope(componentPath_[level]))};
    auto &levelIterator{GetIterator(componentPath_[level])};
    const auto &levelEndIterator{GetTypeSymbol(componentPath_[level])
                                     .template get<DerivedTypeDetails>()
                                     .componentNames()
                                     .cend()};

    while (!descend && levelIterator != levelEndIterator) {
      const Symbol *component{GetComponentSymbol(componentPath_[level])};

      switch (state) {
      case ComponentVisitState::Resume:
        if (StopAtComponentPre<componentKind>(DEREF(component))) {
          // The symbol was not yet considered for
          // traversal.
          descend = PlanComponentTraversal(*component);
        }
        break;
      case ComponentVisitState::Pre:
        // Search iterator
        if (auto iter{scope.find(*levelIterator)}; iter != scope.cend()) {
          const Symbol *newComponent{iter->second};
          SetComponentSymbol(componentPath_[level], newComponent);
          if (StopAtComponentPre<componentKind>(*newComponent)) {
            return;
          }
          descend = PlanComponentTraversal(*newComponent);
          if (!descend && StopAtComponentPost<componentKind>(*newComponent)) {
            return;
          }
        }
        break;
      case ComponentVisitState::Post:
        if (StopAtComponentPost<componentKind>(DEREF(component))) {
          return;
        }
        break;
      }

      if (descend) {
        level++;
      } else {
        SetComponentSymbol(componentPath_[level], nullptr);  // safety
        levelIterator++;
      }
      state = ComponentVisitState::Pre;
    }

    if (!descend) {  // Finished level traversal
      componentPath_.pop_back();
      --level;
      state = ComponentVisitState::Post;
    }
  }
  // iterator reached end of components
}

template<ComponentKind componentKind>
std::string
ComponentIterator<componentKind>::const_iterator::BuildResultDesignatorName()
    const {
  std::string designator{""};
  for (const auto &node : componentPath_) {
    designator += "%" + GetComponentSymbol(node)->name().ToString();
  }
  return designator;
}

template class ComponentIterator<ComponentKind::Ordered>;
template class ComponentIterator<ComponentKind::Direct>;
template class ComponentIterator<ComponentKind::Ultimate>;
template class ComponentIterator<ComponentKind::Potential>;

UltimateComponentIterator::const_iterator FindCoarrayUltimateComponent(
    const DerivedTypeSpec &derived) {
  UltimateComponentIterator ultimates{derived};
  return std::find_if(ultimates.begin(), ultimates.end(),
      [](const Symbol *component) { return DEREF(component).Corank() > 0; });
}

PotentialComponentIterator::const_iterator FindEventOrLockPotentialComponent(
    const DerivedTypeSpec &derived) {
  PotentialComponentIterator potentials{derived};
  return std::find_if(
      potentials.begin(), potentials.end(), [](const Symbol *component) {
        if (const auto *details{
                DEREF(component).detailsIf<ObjectEntityDetails>()}) {
          const DeclTypeSpec *type{details->type()};
          return type && IsEventTypeOrLockType(type->AsDerived());
        }
        return false;
      });
}

const Symbol *FindUltimateComponent(const DerivedTypeSpec &derived,
    std::function<bool(const Symbol &)> predicate) {
  UltimateComponentIterator ultimates{derived};
  if (auto it{std::find_if(ultimates.begin(), ultimates.end(),
          [&predicate](const Symbol *component) -> bool {
            return predicate(DEREF(component));
          })}) {
    return *it;
  }
  return nullptr;
}

bool IsFunctionResult(const Symbol &symbol) {
  return (symbol.has<semantics::ObjectEntityDetails>() &&
             symbol.get<semantics::ObjectEntityDetails>().isFuncResult()) ||
      (symbol.has<semantics::ProcEntityDetails>() &&
          symbol.get<semantics::ProcEntityDetails>().isFuncResult());
}

bool IsFunctionResultWithSameNameAsFunction(const Symbol &symbol) {
  if (IsFunctionResult(symbol)) {
    if (const Symbol * function{symbol.owner().symbol()}) {
      return symbol.name() == function->name();
    }
  }
  return false;
}

}
