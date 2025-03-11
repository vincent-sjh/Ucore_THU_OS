# lab1

## 功能实现

实现了数据结构syscall_num[450]的定义和初始化以及更新，用于记录和维护每个种类的systemcall的调用次数。

还实现了sys_trace函数，用来根据地址返回或者更新数值，以及返回指定id的syscall的调用次数，数据来源是之前维护的数据结构。

## 问答题

1. 正确进入 U 态后，程序的特征还应有：使用 S 态特权指令，访问 S 态寄存器后会报错。请同学们可以自行测试这些内容（参考 [前三个测例](https://github.com/LearningOS/uCore-Tutorial-Test-2025S/tree/main/src) ，描述程序出错行为，同时注意注明你使用的 sbi 及其版本。
   执行三个错例得到如下报错信息。

   ```
   # __ch2_bad_address.c
   [ERROR 0]unknown trap: 0x0000000000000007, stval = 0x0000000000000000 sepc = 0x0000000080400004
   #__ch2_bad_instruction.c
   [rustsbi-panic] hart 0 panicked at 'invalid instruction, mepc: 0000000080400004, instruction: 0000000010200073', platform/qemu/src/main.rs:458:17
   [rustsbi-panic] system shutdown scheduled due to RustSBI panic
   #__ch2_bad_register.c
   [rustsbi-panic] hart 0 panicked at 'invalid instruction, mepc: 0000000080400004, instruction: 00000000100027f3', platform/qemu/src/main.rs:458:17
   [rustsbi-panic] system shutdown scheduled due to RustSBI panic
   ```
2. 请结合用例理解 [trampoline.S](https://github.com/LearningOS/uCore-Tutorial-Code-2025S/blob/ch3/os/trampoline.S) 中两个函数 userret 和 uservec 的作用，并回答如下几个问题:

   1. L79: 刚进入 userret 时，a0、a1 分别代表了什么值。

      a0为保存中断产生的时候U态的trapframe地址，a1为虚拟内存传递用户态进程页表的地址。
   2. L87-L88: sfence 指令有何作用？为什么要执行该指令，当前章节中，删掉该指令会导致错误吗？

      ```
      csrw satp, a1
      sfence.vma zero, zero
      ```

      sfence.vma命令用于刷新快表的TLB表项，页表保存着虚拟内存和物理内存的对应关系，在进行进程切换时对应关系发生变化，从而原来的页表失效，需要进行刷新。当前章节没有实现页表机制，所以说删除无影响。
   3. L96-L125: 为何注释中说要除去 a0？哪一个地址代表 a0？现在 a0 的值存在何处？

      ```
      # restore all but a0 from TRAPFRAME
      ld ra, 40(a0)
      ld sp, 48(a0)
      ld t5, 272(a0)
      ld t6, 280(a0)
      ```

      因为在 `uservec` 中是 `a0` 储存 `trapframe` 的地址用于访问 ，之后需要恢复 `a0` 并存储 `trapframe` 的地址。`112(a0)` 代表寄存器 `a0`，当前 `sscratch` 中保存着 `a0` 的值。
   4. userret：中发生状态切换在哪一条指令？为何执行之后会进入用户态？发生在 `sret` 指令。该指令会设置 S 态为 U 态，并将PC设置为 `sepc` 值，恢复用户态的执行。
   5. L29： 执行之后，a0 和 sscratch 中各是什么值，为什么？

      ```
      csrrw a0, sscratch, a0
      ```

      `a0` 变为 `trapframe` 的地址，而 `sscratch` 变为原来的 `a0` 寄存器的值。在 `userret` 中进入 U 态之前，`sscratch` 被设置为 `trapframe` 的地址， `csrrw a0, sscratch, a0` 指令可以交换 `a0` 和 `sscratch` 的值。
   6. L32-L61: 从 trapframe 第几项开始保存？为什么？是否从该项开始保存了所有的值，如果不是，为什么？

      ```
      sd ra, 40(a0)
      sd sp, 48(a0)
      ...
      sd t5, 272(a0)
      sd t6, 280(a0)
      ```

      对应trapframe结构体的定义， `sd ra, 40(a0)` 从第六项 `ra` 开始保存。目的是保存发生 `trap` 时的各个寄存器的值。没有保存 `a0`是因为此时 `a0` 的值是 `trapframe` 的地址，而不是发生陷入时的 `a0` 的值（被暂存在了 `sscratch` 寄存器）。
   7. 进入 S 态是哪一条指令发生的？ecall指令。
   8. L75-L76: ld t0, 16(a0) 执行之后，`t0`中的值是什么，解释该值的由来？

   > ```
   > ld t0, 16(a0)
   > jr t0
   > ```
   >
   > 16（a0)在trapframe结构体中是kernel_trap的值，trap.c中 `kernel_trap` 被设置为了 `usertrap` 的地址。
   >

## 荣誉准则

1. 在完成本次实验的过程（含此前学习的过程）中，我曾分别与 **以下各位** 就（与本次实验相关的）以下方面做过交流，还在代码中对应的位置以注释形式记录了具体的交流对象及内容：

   > 无
   >
2. 此外，我也参考了 **以下资料** ，还在代码中对应的位置以注释形式记录了具体的参考来源及内容：

   > 无
   >
3. 我独立完成了本次实验除以上方面之外的所有工作，包括代码与文档。 我清楚地知道，从以上方面获得的信息在一定程度上降低了实验难度，可能会影响起评分。
4. 我从未使用过他人的代码，不管是原封不动地复制，还是经过了某些等价转换。 我未曾也不会向他人（含此后各届同学）复制或公开我的实验代码，我有义务妥善保管好它们。 我提交至本实验的评测系统的代码，均无意于破坏或妨碍任何计算机系统的正常运转。 我清楚地知道，以上情况均为本课程纪律所禁止，若违反，对应的实验成绩将按“-100”分计。
