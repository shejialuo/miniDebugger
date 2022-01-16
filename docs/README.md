# miniDebugger

## Fork-Exec

调试器的基本思路是首先要实现单个断点。当实现了单个断点的功能后，后续功能只是在此基础上的工作。由于实现的功能较多，本文采用c++实现，首先是在`miniDebugger.cpp`中构建最基础的部分。

```cpp
int main(int argc, char *argv[]) {
  pid_t pid = fork();
  if(pid == 0) {
    ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);
    // 执行子进程
  } else if (pid > 0) {
    // 调试器实现
  }
}
```

`ptrace`系统调用通过在第一个参数传递`PTRACE_TRACEME`实现process trace的功能。

## 断点功能实现

断点功能实现可谓是调试器中最关键的一个部分，无论是step in, step over, step out都是在断点功能已经实现的基础上进行的扩展。

断点的实现原理是通过`ptrace(PTRACE_PEEKDATA, pid, address, nullptr)`获取`address`的指令内容。通过将`address`的指令内容的后8位改为`INT3`指令，并使用`ptrace(PTRACE_POKEDATA, pid, address, value)`使其陷入(`trap`)。后当需要执行`continue`操作后，需要将指令的内容后8位进行还原。其中修改指令的操作如下：

```cpp
// 取得address的内容
auto data = ptrace(PTRACE_PEEKDATA, pid, address, nullptr);
// 保存后8位
uint8_t savedData = static_cast<uint8_t>(data & 0xff)
uint_t64 dataWithINT3 = (data & ~0xff) | 0xcc
ptrace(PTRACE_POKEDATA, pid, address, dataWithINT3)
```

可以见得，最基本的断点功能实现是对单个指令实现断点。这也是step in, step over, step out实现的基础。

## 寄存器和内存的读写

寄存器和内存的读写实现是比较简单的。寄存器可以直接通过`ptrace(PTRACE_GETREGS, pid, nullptr, &regs)`即可获取，至于其写也可以直接通过ptrace(PTRACE_SETREGS, pid, nullptr, &regs)`实现。内存的读写也比较简单，此处不赘述。下面为一部分函数的原型。

+ `uint64_t getRegisterValue(Reg r);`: 获取寄存器`r`的值
+ `void setRegisterValue(Reg r, uint64_t value)`: 设置寄存器`r`的值

## 信号量的捕获

每当执行相关的`PTRACE`操作后，都会有信号量的产生发送到父进程，故需要采用一个比较优雅的信号处理函数来解决相关的操作，同时只有当接收到信号量后，父进程才能继续执行。故第一步需要实现以下功能：

```cpp
int waitStatus = 0;
int options = 0;
waitpid(pid, &waitStatus, options)
```

然后，需要对信号进行处理，通过使用`PTRACE(PTRACE_GETSIGINFO, pid, nullptr, &info)`来得到`siginfo_t`类型的`info`的值。此处，我们只关注`SIGTRAP`信号。其余的信号均作为差错处理。

```cpp
siginfo_t info;
PTRACE(PTRACE_GETSIGINFO, pid, nullptr, &info)

switch(info.si_signo) {
case SIGTRAP: //我们只关心SIGTRAP信号
  handleSignaltrap(info);
}
```

在信号处理函数`handleSignaltrap(info)`中，我们需要进行以下的操作，由于`INT3`指令的大小为1bit。故需要获取当前PC，即寄存器`rip`的值。将其-1指向原有的地址。

```cpp
uint64_t getPC() {
  getRegisterValue(rip);
}
void setPC() {
  setRegisterValue(rip, value;)
}
setPC(getPC() - 1);
```

经过这样的操作即可以使得PC的状态的得以返回。

## 从断点继续

显然，从断点继续需要进行两个操作：

+ 还原地址指向的值的内容
+ 还原PC

在1.3和1.4节实现了这个操作，故本文定义了函数`stepOverBreakpoint`

```cpp
void stepOverBreakpoint() {
  breakPoint.disabled();
  waitForSignal();
}
```

## 打印断点处的源码

DWARF提供了丰富的信息用于Debug的信息。其本质是一个树的数据结构，存储了变量名，函数名，通过基本的类型组合成复杂的类型。其每一个Compilation Unit均对应了一个Debug Information Entry (DIE)，其中的line表对应了源代码所在的行，由于每一行可能包含多个指令，故其采用了状态机的方式构建。由于处理DWARF极其复杂，本文采用了已有库解决解析问题。

由于解析所需的地址均为相对地址，故需要从`/proc/<pid>/maps`找寻加载到内存的虚拟地址。本文采用了比较粗暴单一的方式解决。

```cpp
void Debugger::initializeLoadAddress() {
  if(pElf.get_hdr().type == elf::et::dyn) {
    std::ifstream map("/proc/" + std::to_string(pid) + "/maps");
    std::string address;
    std::getline(map, address, '-');
    map.close();
    spdlog::info("The load address is {}", address);
    loadAddress = std::stol(address, 0, 16);
  }
}
```

利用已有的库封装两个函数：

```cpp
dwarf::die Debugger::getFunctionFromPC(uint64_t pc);

dwarf::line_table::iterator Debugger::getLineEntryFromPC(uint64_t pc);
```

显然，不需要自己做解析工作了。当找到了源代码位于的段，就可以打印该行的上下文。其过程并不复杂，只需要维持一个窗口即可。

## 实现代码级别的跳转

在上述的实现过程中，都是实现的指令级别。在实际的调试过程中，我们一般需要进行三种不同类型的操作，step in, step out, step over.

### 单指令

单指令的实现很简单。对于已经加了断点的指令。直接执行已经实现的`stepOverBreakpoint`。对于未加断连的，直接执行`ptrace(PTRACE_SINGLESTEP, pid, nullptr, nullptr)`。

### Step out

`Step out`的实现比较简单，直接去栈寄存器的值，对其+8,找到返回地址。然后判断用户是否已经在该地址处设置断点，如果设置了，直接`continueExecution`。如果未设置，需要先设置断点，然后将其删除。

```cpp
void Debugger::stepOut() {
  auto framePointer = memory.getRegisterValue(Reg::rbp);
  auto returnAddress = memory.readMemory(framePointer + 8);

  bool shouldRemoveBreakpoint = false;
  if(!breakpoints.count(returnAddress)) {
    setBreakPointAtAddress(returnAddress);
    shouldRemoveBreakpoint = true;
  }

  continueExecution();

  if(shouldRemoveBreakpoint) {
    removeBreakpoint(returnAddress);
  }
}
```

### Step in

对于源代码的每一行可能包含许多条指令，step in就是从源代码的一行跳转到下一行或者其对应的函数，然而我们需要通过单指令的方式实现代码行数级别的跳转。显然的思路就是通过DWARF当行数没有变化时，执行单指令的跳转。然后打印出目前PC指向的源码：

```cpp
void Debugger::stepIn() {
  auto line = getLineEntryFromPC(getOffsetPC())->line;
  while(getLineEntryFromPC(getOffsetPC())->line == line) {
    singleStepInstructionWithBreakpointCheck();
  }
  auto lineEntry = getLineEntryFromPC(getOffsetPC());
  printSource(lineEntry->file->path, lineEntry->line);
}
```

### Step over

Step over的一个直观的做法就是把断点打在下一行的源码。但是有个问题，这个下一行的源码是很难确定的，如果目前是位于循环或者条件中呢？实际的调试器会检查所有的分支，确定跳出的地址可能在何处，然后对可能跳出的地址全部打上断点。

本项目采用一个极其暴力的做法，全部加上断点即可。

## 代码级别断点

目前，本项目只实现了从地址出打断点，不具有抽象性。故需要实现代码级别的断点，总共具备以下三种方式：

+ `0x<hexadecimal>`
+ `<filename>:<line>`
+ `function name`

### 函数名实现

由于DWARF的`DW_TAG_subprogram`中保存了`DW_AT_name`直接查询即可。通过在`lower_pc`字段加入断点，即可实现通过函数名打断点。

```cpp
void Debugger::setBreakPointAtFunction(const std::string& name) {
  for(const auto& compilationUnit: pDwarf.compilation_units()) {
    for(const auto& die: compilationUnit.root()) {
      if(die.has(dwarf::DW_AT::name) && at_name(die) == name) {
        auto lowPC = at_low_pc(die);
        auto entry = getLineEntryFromPC(lowPC);
        ++entry; //skip prologue
        setBreakPointAtAddress(offsetDwarfAddress(entry->address));
      }
    }
  }
}
```

### 通过行数实现

仍然是通过`DWARF`查询，由于编译的单元可以来自于不同的文件，故需要加上文件名。

```cpp
void Debugger::setBreakPointAtSouceLine(const std::string& file, unsigned line) {
  for (const auto& compilationUnit: pDwarf.compilation_units()) {
    if(isSuffix(file, at_name(compilationUnit.root()))) {
      const auto& lt = compilationUnit.get_line_table();

      for(const auto& entry: lt) {
        if(entry.is_stmt && entry.line == line) {
          setBreakPointAtAddress(offsetDwarfAddress(entry.address));
          return;
        }
      }
    }
  }
}
```
