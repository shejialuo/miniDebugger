/*
  * x86_64 has sets of general and special purpose registers.
  * Also, it has floating point and vector registers available.
  * In this debugger, floating point and vector registers are omitted.
  * We can access 64 bit registers as 32, 16, or 8 bit registers.
  * However, only 64 bit is used for simplification.
*/

#ifndef REG_H
#define REG_H

#include <iostream>
#include <string>
#include <array>
#include "sys/user.h"

enum class Reg {
  rax, rbx, rcx, rdx,
  rdi, rsi, rbp, rsp,
  r8,  r9,  r10, r11,
  r12, r13, r14, r15,
  rip, rflags,    cs,
  orig_rax,  fs_base,
  gs_base,
  fs, gs, ss, ds, es
};

constexpr std::size_t registersNumber = 27;

struct RegDescriptor {
  Reg reg;
  int dwarfReg;
  std::string name;
};

/*
  * This array is mapped with "sys/user.h" for making code
  * not use ugly switch and use STL algorithms
*/
const std::array<RegDescriptor, registersNumber> Registers {{
  { Reg::r15, 15, "r15" },
  { Reg::r14, 14, "r14" },
  { Reg::r13, 13, "r13" },
  { Reg::r12, 12, "r12" },
  { Reg::rbp, 6, "rbp" },
  { Reg::rbx, 3, "rbx" },
  { Reg::r11, 11, "r11" },
  { Reg::r10, 10, "r10" },
  { Reg::r9, 9, "r9" },
  { Reg::r8, 8, "r8" },
  { Reg::rax, 0, "rax" },
  { Reg::rcx, 2, "rcx" },
  { Reg::rdx, 1, "rdx" },
  { Reg::rsi, 4, "rsi" },
  { Reg::rdi, 5, "rdi" },
  { Reg::orig_rax, -1, "orig_rax" },
  { Reg::rip, -1, "rip" },
  { Reg::cs, 51, "cs" },
  { Reg::rflags, 49, "eflags" },
  { Reg::rsp, 7, "rsp" },
  { Reg::ss, 52, "ss" },
  { Reg::fs_base, 58, "fs_base" },
  { Reg::gs_base, 59, "gs_base" },
  { Reg::ds, 53, "ds" },
  { Reg::es, 50, "es" },
  { Reg::fs, 54, "fs" },
  { Reg::gs, 55, "gs" },
}};

#endif // REG_H
