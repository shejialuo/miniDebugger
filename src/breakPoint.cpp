#include "breakpoint.h"

#include "sys/ptrace.h"

#include <cstdint>

Breakpoint::Breakpoint(pid_t p, std::intptr_t addr) : pid{p}, address{addr}, enabled{false}, savedData{} {}

void Breakpoint::enable() {
  // Use `PTRACE_PEEKDATA` to read the content of the specified address
  auto data = ptrace(PTRACE_PEEKDATA, pid, address, nullptr);
  // Store the lower 8 bit
  savedData = static_cast<uint8_t>(data & 0xff);
  uint64_t int3 = 0xcc;
  uint64_t dataWithInt3 = (data & ~0xff) | int3;

  // Substitute the low 8bit to the original data to achieve interrupt operation
  // Use `PTRACE_POKEDATA` to rewrite the content.
  ptrace(PTRACE_POKEDATA, pid, address, dataWithInt3);
  enabled = true;
}

void Breakpoint::disable() {
  auto data = ptrace(PTRACE_PEEKDATA, pid, address, nullptr);
  // recover the lower 8 bit
  auto restoredData = (data & ~0xff) | savedData;
  ptrace(PTRACE_POKEDATA, pid, address, restoredData);
  enabled = false;
}
