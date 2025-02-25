! Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

! Test character intrinsic function folding

module character_intrinsic_tests
  logical, parameter :: test_char1_ok1 = char(0_4, 1).EQ." "
  logical, parameter :: test_char1_ok2 = char(127_4, 1).EQ.""
  logical, parameter :: test_char1_ok3 = char(97_4, 1).EQ."a"
  logical, parameter :: test_char1_ok4 = .NOT.char(97_4, 1).EQ."b"
  logical, parameter :: test_char1_ok5 = char(355_4, 1).EQ."c"
  logical, parameter :: test_char1_ok6 = char(-61_1, 1).EQ.char(195_4, 1)

  logical, parameter :: test_char2_ok1 = char(0_4, 2).EQ.2_" "
  logical, parameter :: test_char2_ok2 = char(127_4, 2).EQ.2_""
  logical, parameter :: test_char2_ok3 = char(122_4, 2).EQ.2_"z"

  logical, parameter :: test_char4_ok1 = char(0, 4).EQ.4_" "
  logical, parameter :: test_char4_ok2 = char(INT(12435, 4), 4).EQ.4_"ん"
  logical, parameter :: test_char4_ok3 = char(354_4, 4).EQ.4_"Ţ"
  logical, parameter :: test_char4_ok4 = char(-1_1, 4).EQ.char(x'ffffffff', 4)

  character(kind=4, len=:), parameter :: c4aok(*) = char([97_4, 98_4, 99_4, 20320_4, 22909_4], 4)
  logical, parameter :: test_char4_array = (c4aok(1).EQ.4_"a").AND.(c4aok(2).EQ.4_"b") &
    .AND.(c4aok(3).EQ.4_"c").AND.(c4aok(4).EQ.4_"你").AND.(c4aok(5).EQ.4_"好")

  logical, parameter :: test_achar4_ok1 = achar(0_4, 4).EQ.4_" "
  logical, parameter :: test_achar4_ok2 = achar(127_4, 4).EQ.4_""

  character(kind=1, len=:), parameter :: c1aok(*) = achar([97_4, 0_4, 98_4], 1)
  logical, parameter :: test_char1_array = (c1aok(1).EQ.1_"a").AND.(c1aok(2).EQ.1_" ") &
    .AND.(c1aok(3).EQ.1_"b")


  logical, parameter :: test_ichar1 = char(ichar("a")).EQ."a"
  logical, parameter :: test_ichar2 = ichar(char(255)).EQ.255
  logical, parameter :: test_ichar3 = ichar(char(-1_1), 1).EQ.-1_1
  logical, parameter :: test_ichar4 = ichar(char(2147483647_4, 4), 4).EQ.2147483647_4
  logical, parameter :: test_ichar5 = ichar(char(4294967295_8, 4), 8).EQ.4294967295_8
  logical, parameter :: test_ichar6 = ichar(char(4294967296_8, 4), 8).EQ.0_8
  logical, parameter :: test_iachar1 = achar(iachar("a")).EQ."a"
  logical, parameter :: test_iachar2 = iachar(achar(22)).EQ.22
  logical, parameter :: test_iachar3 = ichar(char(-2147483647_4, 4), 4).EQ.(-2147483647_4)
  logical, parameter :: test_iachar5 = ichar(char(65535_4, 2), 4).EQ.65535_4
  logical, parameter :: test_iachar6 = ichar(char(65536_4, 2), 4).EQ.0_4

  ! Not yet recognized as intrinsic
  !character(kind=1, len=1), parameter :: test_c1_new_line = new_line("a")

  logical, parameter :: test_c1_adjustl1 = adjustl("  this is a test").EQ.("this is a test  ")
  logical, parameter :: test_c1_adjustl2 = .NOT."  this is a test".EQ.("this is a test  ")
  logical, parameter :: test_c1_adjustl3 = adjustl("").EQ.("")
  logical, parameter :: test_c1_adjustl4 = adjustl("that").EQ.("that")
  logical, parameter :: test_c1_adjustl5 = adjustl("that ").EQ.("that ")
  logical, parameter :: test_c1_adjustl6 = adjustl(" that ").EQ.("that  ")
  logical, parameter :: test_c1_adjustl7 = adjustl("    ").EQ.("    ")
  character(kind=1, len=:), parameter :: c1_adjustl8(*) = adjustl(["  this is a test", " that is a test "])
  logical, parameter :: test_c1_adjustl8 = (c1_adjustl8(1).EQ.1_"this is a test  ").AND.(c1_adjustl8(2).EQ.1_"that is a test  ")

  logical, parameter :: test_c4_adjustr1 = adjustr(4_"  你好吗 ? ").EQ.(4_"   你好吗 ?")
  logical, parameter :: test_c4_adjustr2 = .NOT.(4_"  你好吗 ? ").EQ.(4_"  你好吗 ?")
  logical, parameter :: test_c4_adjustr3 = adjustr(4_"").EQ.(4_"")
  logical, parameter :: test_c4_adjustr4 = adjustr(4_"   ").EQ.(4_"   ")
  logical, parameter :: test_c4_adjustr5 = adjustr(4_"你 好吗?").EQ.(4_"你 好吗?")
  logical, parameter :: test_c4_adjustr6 = adjustr(4_" 你好吗?").EQ.(4_" 你好吗?")
  logical, parameter :: test_c4_adjustr7 = adjustr(4_" 你好吗? ").EQ.(4_"  你好吗?")
  character(kind=4, len=:), parameter :: c4_adjustr8(*) = adjustr([4_"  你 好吗?%%% ", 4_"你a好b 吗c?   "])
  logical, parameter :: test_c4_adjustr8 = (c4_adjustr8(1).EQ.4_"   你 好吗?%%%").AND.(c4_adjustr8(2).EQ.4_"   你a好b 吗c?")

end module
