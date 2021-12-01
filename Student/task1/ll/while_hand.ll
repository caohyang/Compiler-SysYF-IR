; ModuleID = 'while_test.c'
source_filename = "while_test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@b = common dso_local global i32 0, align 4       ; b is the i32 global variable
@a = common dso_local global i32 0, align 4       ; a is the i32 global variable

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
    %1 = alloca i32, align 4                      ; return value of main()
    store i32 0, i32* %1, align 4                 ; return value of main() = 0
    store i32 0, i32* @b, align 4                 ; b = 0
    store i32 3, i32* @a, align 4                 ; a = 3
    br label %2                                   ; branch label 2

2:                                                ; preds = %5, %0
    %3 = load i32, i32* @a, align 4               ; i32 %3 = a
    %4 = icmp sgt i32 %3, 0                       ; i1 %4 = %3>0
    br i1 %4, label %5, label %11                 ; if(%4==1) branch label 5; else branch label 11

5:                                                ; preds = %2
    %6 = load i32, i32* @b, align 4               ; i32 %6 = b
    %7 = load i32, i32* @a, align 4               ; i32 %7 = a
    %8 = add nsw i32 %6, %7                       ; i32 %8 = a + b
    store i32 %8, i32* @b, align 4                ; b = %8 = a+b
    %9 = load i32, i32* @a, align 4               ; i32 %9 = a
    %10 = sub nsw i32 %9, 1                       ; i32 %10 = %9-1 = a-1
    store i32 %10, i32* @a, align 4               ; a = %10 = a-1
    br label %2                                   ; branch label 2

11:                                               ; preds = %2
    %12 = load i32, i32* @b, align 4              ; i32 %12 = b
    ret i32 %12                                   ; return b
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.1 "}
