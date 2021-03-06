; ModuleID = 'func_test.c'
source_filename = "func_test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @add(i32 %0, i32 %1) #0 {
    %3 = alloca i32, align 4               ; int a
    %4 = alloca i32, align 4               ; int b
    store i32 %0, i32* %3, align 4         ; a = i32 %0
    store i32 %1, i32* %4, align 4         ; b = i32 %1
    %5 = load i32, i32* %3, align 4        ; i32 %5 = i32* %3 = a
    %6 = load i32, i32* %4, align 4        ; i32 %6 = i32* %4 = b
    %7 = add nsw i32 %5, %6                ; i32 %7 = i32 %5 + i32 %6 = a + b
    %8 = sub nsw i32 %7, 1                 ; i32 %8 = i32 %7 - 1 = a + b -1
    ret i32 %8                             ; return a+b-1
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
    %1 = alloca i32, align 4               ; return value of main()
    %2 = alloca i32, align 4               ; int a
    %3 = alloca i32, align 4               ; int b
    %4 = alloca i32, align 4               ; int c
    store i32 0, i32* %1, align 4          ; return value of main() = 0
    store i32 3, i32* %2, align 4          ; a = 3
    store i32 2, i32* %3, align 4          ; b = 2
    store i32 5, i32* %4, align 4          ; c = 5
    %5 = load i32, i32* %4, align 4        ; i32 %5 = i32* %4 = c = 5
    %6 = load i32, i32* %2, align 4        ; i32 %6 = i32* %2 = a = 3
    %7 = load i32, i32* %3, align 4        ; i32 %7 = i32* %3 = b = 2
    %8 = call i32 @add(i32 %6, i32 %7)     ; call add(i32 %6, i32%7) function, return a+b 
    %9 = add nsw i32 %5, %8                ; c + (a+b)
    ret i32 %9                             ; return c + (a+b)
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.1 "}
