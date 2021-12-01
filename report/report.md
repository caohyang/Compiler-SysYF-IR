# PW6 实验报告

学号：PB19000046    姓名：曹奕阳

学号：PB19000254    姓名：薛东昀

学号：PB19000372    姓名：孙杰



## 问题回答

### Part 1

1. 请给出while语句对应的LLVM IR的代码布局特点，重点解释其中涉及的几个`br`指令的含义（包含各个参数的含义）

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

   > LLVM IR 的函数调用语句与高级语言的函数调用几乎没有区别，使用`call`指令就可以像高级语言那样直接调用函数并完成以下功能
   >
   > - 传递参数
   > - 执行函数
   > - 获得返回值

### Part 2



### Part 3



## 实验设计



## 实验难点及解决方案



## 实验总结



## 实验反馈



## 组间交流

