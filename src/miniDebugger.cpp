#include <debugger.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <sys/ptrace.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    spdlog::error("Program name out specified");
    return -1;
  }
  auto programName = argv[1];

  auto pid = fork();

  if (pid == 0) {
    // `PTRACE_TRACEME` indicates that this process should
    // allow its parent to trace it. And it would send a
    // signal to the process.
    ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
    execl(programName, programName, nullptr);
  } else if (pid > 0) {
    spdlog::info("Start debugging process {}", pid);
    Debugger debugger{programName, pid};
    debugger.run();
  } else {
    spdlog::error("Fork Error");
  }
}
