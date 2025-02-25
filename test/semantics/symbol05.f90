! Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
!
! Licensed under the Apache License, Version 2.0 (the "License");
! you may not use this file except in compliance with the License.
! You may obtain a copy of the License at
!
!     http://www.apache.org/licenses/LICENSE-2.0
!
! Unless required by applicable law or agreed to in writing, software
! distributed under the License is distributed on an "AS IS" BASIS,
! WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
! See the License for the specific language governing permissions and
! limitations under the License.

! Explicit and implicit entities in blocks

!DEF: /s1 Subprogram
subroutine s1
 !DEF: /s1/x ObjectEntity INTEGER(4)
 integer x
 block
  !DEF: /s1/Block1/y ObjectEntity INTEGER(4)
  integer y
  !REF: /s1/x
  x = 1
  !REF: /s1/Block1/y
  y = 2.0
 end block
 block
  !DEF: /s1/Block2/y ObjectEntity REAL(4)
  real y
  !REF: /s1/Block2/y
  y = 3.0
 end block
end subroutine

!DEF: /s2 Subprogram
subroutine s2
 implicit integer(w-x)
 block
  !DEF: /s2/x (implicit) ObjectEntity INTEGER(4)
  x = 1
  !DEF: /s2/y (implicit) ObjectEntity REAL(4)
  y = 2
 end block
contains
 !DEF: /s2/s Subprogram
 subroutine s
  !REF: /s2/x
  x = 1
  !DEF: /s2/s/w (implicit) ObjectEntity INTEGER(4)
  w = 1
 end subroutine
end subroutine

!DEF: /s3 Subprogram
subroutine s3
 !DEF: /s3/j ObjectEntity INTEGER(8)
 integer(kind=8) j
 block
  !DEF: /s3/Block1/t DerivedType
  type :: t
   !DEF: /s3/Block1/t/x ObjectEntity REAL(4)
   !DEF: /s3/Block1/t/ImpliedDos1/i (implicit) ObjectEntity INTEGER(4)
   real :: x(10) = [(i, i=1,10)]
   !DEF: /s3/Block1/t/y ObjectEntity REAL(4)
   !DEF: /s3/Block1/t/ImpliedDos2/j ObjectEntity INTEGER(8)
   real :: y(10) = [(j, j=1,10)]
  end type
 end block
end subroutine

!DEF: /s4 Subprogram
subroutine s4
 implicit integer(x)
 interface
  !DEF: /s4/s EXTERNAL Subprogram
  !DEF: /s4/s/x (implicit) ObjectEntity REAL(4)
  !DEF: /s4/s/y (implicit) ObjectEntity INTEGER(4)
  subroutine s (x, y)
   implicit integer(y)
  end subroutine
 end interface
end subroutine

!DEF: /s5 Subprogram
subroutine s5
 block
  !DEF: /s5/Block1/x (implicit) ObjectEntity REAL(4)
  dimension :: x(2)
  block
   !DEF: /s5/Block1/Block1/x (implicit) ObjectEntity REAL(4)
   dimension :: x(3)
  end block
 end block
 !DEF: /s5/x (implicit) ObjectEntity REAL(4)
 x = 1.0
end subroutine

!DEF: /s6 Subprogram
subroutine s6
  !DEF: /s6/i ObjectEntity INTEGER(4)
  !DEF: /s6/j ObjectEntity INTEGER(4)
  !DEF: /s6/k ObjectEntity INTEGER(4)
  integer i, j, k
  block
    !DEF: /s6/Block1/i ASYNCHRONOUS, VOLATILE HostAssoc INTEGER(4)
    volatile :: i
    !DEF: /s6/Block1/j ASYNCHRONOUS HostAssoc INTEGER(4)
    asynchronous :: j
    !REF: /s6/Block1/i
    asynchronous :: i
    !DEF: /s6/Block1/k TARGET(implicit) ObjectEntity INTEGER(4)
    target :: k
  end block
end subroutine

!DEF: /m7 Module
module m7
  !DEF: /m7/i PUBLIC ObjectEntity INTEGER(4)
  !DEF: /m7/j PUBLIC ObjectEntity INTEGER(4)
  integer i, j
end module
!DEF: /s7 Subprogram
subroutine s7
  !REF: /m7
  use :: m7
  !DEF: /s7/j VOLATILE Use INTEGER(4)
  volatile :: j
end subroutine
