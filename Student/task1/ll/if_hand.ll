; ModuleID = 'if_test.c'
source_filename = "if_test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@a = common dso_local global i32 0, align 4         ; a is the i32 global variable

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
    %1 = alloca i32, align 4                        ; return value of main()
    store i32 0, i32* %1, align 4                   ; return value of main() = 0
    store i32 10, i32* @a, align 4                  ; i32* @a = 10, a is the globle variable
    %2 = load i32, i32* @a, align 4                 ; i32 %2 = i32* @a = 10
    %3 = icmp sgt i32 %2, 0                         ; i1 %3 = (%2>0)
    br i1 %3, label %4, label %6                    ; if (%3==1) branch label 4; else branch label 6

4:                                                  ; preds = %0
    %5 = load i32, i32* @a, align 4                 ; i32 %5 = i32* @a
    store i32 %5, i32* %1, align 4                  ; return value *%1 = %5
    br label %7                                     ; branch label 7

6:                                                  ; preds = %0
    store i32 0, i32* %1, align 4                   ; return value *%1 = 0
    br label %7                                     ; branch label 7

7:                                                  ; preds = %6, %4
    %8 = load i32, i32* %1, align 4                 ; return value %8 = i32* %1
    ret i32 %8                                      ; return value %8
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 10.0.1 "}
