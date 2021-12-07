# PW6 实验报告

学号：PB19000046    姓名：曹奕阳

学号：PB19000254    姓名：薛东昀

学号：PB19000372    姓名：孙杰



## 问题回答

### Part 1

1. 请给出while语句对应的LLVM IR的代码布局特点，重点解释其中涉及的几个`br`指令的含义（包含各个参数的含义）

   &nbsp;
   
   > while 语句部分的核心LLVM IR代码如下所示
   >
   > ```c
   > define dso_local i32 @main() #0 {
   >     %1 = alloca i32, align 4                      ; return value of main()
   >     store i32 0, i32* %1, align 4                 ; return value of main() = 0
   >     store i32 0, i32* @b, align 4                 ; b = 0
   >     store i32 3, i32* @a, align 4                 ; a = 3
   >     br label %2                                   ; br label 2
   > 
   > 2:                                                ; preds = %5, %0
   >     %3 = load i32, i32* @a, align 4               ; i32 %3 = a
   >     %4 = icmp sgt i32 %3, 0                       ; i1 %4 = %3>0
   >     br i1 %4, label %5, label %11                 ; if(%4==1) br label 5; else br label 11
   > 
   > 5:                                                ; preds = %2
   >     %6 = load i32, i32* @b, align 4               ; i32 %6 = b
   >     %7 = load i32, i32* @a, align 4               ; i32 %7 = a
   >     %8 = add nsw i32 %6, %7                       ; i32 %8 = a + b
   >     store i32 %8, i32* @b, align 4                ; b = %8 = a+b
   >     %9 = load i32, i32* @a, align 4               ; i32 %9 = a
   >     %10 = sub nsw i32 %9, 1                       ; i32 %10 = %9-1 = a-1
   >     store i32 %10, i32* @a, align 4               ; a = %10 = a-1
   >     br label %2                                   ; br label 2
   > 
   > 11:                                               ; preds = %2
   >     %12 = load i32, i32* @b, align 4              ; i32 %12 = b
   >     ret i32 %12                                   ; return b
   > }
   > ```
   >
   > - 代码布局特点：`label 2` 部分是while循环当中的判断条件，`label 5` 部分是while循环体内部所作操作，`label 11` 部分是得到main语句对应的return值
   > - `br label %2 `：while循环体前的代码执行完之后开始进入while判断条件，即进入`label 2`
   > - `label 2` 中的 `br i1 %4` ：`i1 %4 = icmp sgt i32 %3, 0` 表示说若%3 > 0则%4 = 1否则%4 = 0。 即若%4 == 1，接着循环体内的代码，即转移至`label 5` ；否则退出循环，即转移至`label 11`
   > - `label 5` 中的 `br label %2`：执行完while循环体内的代码后重新判断循环条件是否满足，即进入`label 2`

2. 请简述函数调用语句对应的LLVM IR的代码特点

   &nbsp;
   
   > LLVM IR 的函数调用语句与高级语言的函数调用几乎没有区别，使用`call`指令就可以像高级语言那样直接调用函数并完成以下功能
   >
   > - 传递参数
   > - 执行函数
   > - 获得返回值

### Part 2

1. 请给出`SysYFIR.md`中提到的两种getelementptr用法的区别, 并解释原因:
   - `%2 = getelementptr [10 x i32], [10 x i32]* %1, i32 0, i32 %0` 
   - `%2 = getelementptr i32, i32* %1, i32 %0`

   &nbsp;

   > 在调用getelementptr时，[10 x i32]指针除了参数`i32 %0`表示偏移，还需要在其前面添加参数`i32 0`；而i32指针只需要参数`i32 %0`表示偏移。
   > 原因在于任何一个指针都可以作为一个数组使用来进行指针计算，而这里期望的类型是指针。故[10 x i32]指针需要参数`i32 0`固定指针类型，再使用第二个参数计算数组元素对应的地址。

### Part 3

1. function和variable可以在同一个作用域重名，将function单独处理避免了重名的问题，且单独处理可以节省vector长度，减少处理时间


## 实验设计

将第二关中的所有手写代码程序化，主要的难点在最基本的左值，函数和变量定义的处理上。

## 实验难点及解决方案

1. 在程序各处类型转换的处理，尤其在隐式转换时的处理，处理时有时还需要改变实参因此创建了type_cast函数
2. constant类带来的麻烦，由于实验中使用的大部分为基类Value，该类没有value_属性，因此在计算过程中全部需要转换成Constant类再进行操作
3. 全局变量的维护，包括temp_val将递归返回的值进行记录，require_l_val记录左值的使用。
4. 空label的产生，当条件或循环语句在函数末时产生空label，需要手动加入空返回语句。

## 实验总结

本次实验不但涉及了pw4的AST中各个结构体的使用，并加入了产生中间代码翻译过程中的新的一系列结构体，工程规模极大。由于我们在完全完成工程初版后才进行测试，debug，产生了难以定位错误位置的问题，虽说最终问题都得到了解决，但是浪费了大量时间精力，更合理的方式为在码代码的过程中对已完成的函数进行简单的测试。

## 实验反馈

实验量较大，希望能够有一种给出隐藏关卡是否通过的形式，类似oj

## 组间交流

在第3关的代码书写以及debug过程中通力协作。