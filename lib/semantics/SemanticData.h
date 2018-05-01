// Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef FORTRAN_SEMA_DATA_H_
#define FORTRAN_SEMA_DATA_H_

#include <cassert> 

//
// 
// Declare here the members of the Semantic<T> that will 
// be attached to each parse-tree class T. The default is 
// an empty struct. All members added here shall be 
// copiable and should be provided with a default value.  
//
// Here are a few common fields 
//  
//  Scope *scope_provider  = the scope provided by a construct or statement
//  int stmt_index         = the index used in the StatementMap 
//
// Remark: Weither we want to annotate parse-tree nodes with 
// semantic information is still debatable. 
//

namespace Fortran::semantics {

// Initialize the semantic information attached to a parser-tree node
//
// Ideally, the function should be called once at the begining of the corresponding Pre() 
// member in Pass1. However, for the few cases where the semantic data need to be 
// initialize earlier the strict argument can be set to false. 
//
template <typename T>  Semantic<T> & InitSema(const T &node, bool strict=true) { 

  // Do not use the default implementation!
  // If the following assert fails, then a DECLARE_SEMANTIC_DATA is 
  // missing below
  assert(Semantic<T>::IS_DECLARED);

  if (node.s) {
    if (strict) {
      // TODO: emit proper message
      std::cerr << "Duplicate call of " << __PRETTY_FUNCTION__ << "\n" ;
      exit(1);
    } else {
      return *(node.s); 
    }
  }
  auto s = new Semantic<T>( const_cast<T*>(&node) ) ;
  const_cast<T&>(node).s = s; 
  return *s ; 
} 


// Retreive the semantic information attached to a parser-tree node
template <typename T> Semantic<T> & GetSema(const T &node) { 
  // Do not use the default implementation!
  // If the following assert fails, then a DECLARE_SEMANTIC is missing above 
  assert(Semantic<T>::IS_DECLARED); 
  assert(node.s) ;
  return *(node.s) ;
} 


#define DEFINE_SEMANTIC_DATA(Class) \
  template <> struct Semantic<Fortran::parser::Class> { \
    Semantic<Fortran::parser::Class>(Fortran::parser::Class *node) {} \
    enum {IS_DECLARED=1};
  
#define END_SEMANTIC_DATA \
  }

// Some fields that need to be defined for all statements
#define SEMANTIC_STMT_FIELDS \
   int stmt_index=0 

DEFINE_SEMANTIC_DATA(ProgramUnit)
  StatementMap *statement_map=0 ;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(MainProgram)
  Scope *scope_provider=0 ; 
  LabelTable *label_table=0 ; 
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(SubroutineSubprogram)
  Scope *scope_provider=0 ; 
  LabelTable *label_table=0 ; 
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(FunctionSubprogram)
  Scope *scope_provider=0 ; 
  LabelTable *label_table=0 ; 
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(Module)
  Scope *scope_provider=0 ; 
  LabelTable *label_table=0 ; 
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(DerivedTypeDef)
  // WARNING: there is also a sm::DerivedTypeDef defined in types.h 
  Scope *scope_provider=0 ;
END_SEMANTIC_DATA;


DEFINE_SEMANTIC_DATA(AssignmentStmt)
  SEMANTIC_STMT_FIELDS; 
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(DataStmt)
  SEMANTIC_STMT_FIELDS; 
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(FunctionStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(SubroutineStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ModuleStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;
  
DEFINE_SEMANTIC_DATA(EndModuleStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;
  
DEFINE_SEMANTIC_DATA(StmtFunctionStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndFunctionStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndSubroutineStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(TypeDeclarationStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(DerivedTypeStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndTypeStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(PrintStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(UseStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ProgramStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndProgramStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ImplicitStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(AccessStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(AllocatableStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(AsynchronousStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(BindStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(CodimensionStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ContiguousStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ContainsStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(DimensionStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ExternalStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(IntentStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(IntrinsicStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(NamelistStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(OptionalStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(PointerStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ProtectedStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(SaveStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(TargetStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ValueStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(VolatileStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(CommonStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EquivalenceStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(BasedPointerStmt) // extension
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(GenericStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ParameterStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EnumDef)
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EnumDefStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndEnumStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(InterfaceStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndInterfaceStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(IfThenStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ElseIfStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;
  
DEFINE_SEMANTIC_DATA(ElseStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;
  
DEFINE_SEMANTIC_DATA(EndIfStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(IfStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(SelectCaseStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(CaseStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndSelectStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(SelectRankStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(SelectRankCaseStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(SelectTypeStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ProcedureDeclarationStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(StructureStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(StructureDef::EndStructureStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(FormatStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EntryStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ImportStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(AllocateStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(BackspaceStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(CallStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(CloseStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ContinueStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(DeallocateStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndfileStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EventPostStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EventWaitStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(CycleStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ExitStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(FailImageStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(FlushStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(FormTeamStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(GotoStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(InquireStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(LockStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(NullifyStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(OpenStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(PointerAssignmentStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ReadStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ReturnStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(RewindStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(StopStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(SyncAllStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(SyncImagesStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(SyncMemoryStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(SyncTeamStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(UnlockStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(WaitStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(WhereStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(WriteStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ComputedGotoStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ForallStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;
 
DEFINE_SEMANTIC_DATA(ForallConstructStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndForallStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;
 
DEFINE_SEMANTIC_DATA(ArithmeticIfStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(AssignStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(AssignedGotoStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(PauseStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(PrivateStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(TypeBoundProcedureStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(TypeBoundGenericStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(FinalProcedureStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ComponentDefStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EnumeratorDefStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(TypeGuardStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(NonLabelDoStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(LabelDoStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndDoStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(BlockStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndBlockStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(AssociateStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndAssociateStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(ChangeTeamStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndChangeTeamStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(CriticalStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;

DEFINE_SEMANTIC_DATA(EndCriticalStmt)
  SEMANTIC_STMT_FIELDS;
END_SEMANTIC_DATA;


#undef DEFINE_SEMANTIC_DATA
#undef END_SEMANTIC_DATA_SEMANTIC
#undef SEMANTIC_STMT_FIELDS

} // of namespace Fortran::semantics

#endif // FORTRAN_SEMA_DATA_H_
