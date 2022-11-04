; ModuleID = 'fib.c'
source_filename = "fib.c"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx12.0.0"

@.str = private unnamed_addr constant [3 x i8] c"aa\00", align 1
@zomo = common global i8* null, align 8
@.str.1 = private unnamed_addr constant [3 x i8] c"%s\00", align 1

; Function Attrs: noinline nounwind optnone ssp uwtable
define i64 @fib(i64 %0) #0 {
  %2 = alloca i64, align 8
  %3 = alloca i64, align 8
  store i64 %0, i64* %3, align 8
  %4 = load i64, i64* %3, align 8
  %5 = icmp eq i64 %4, 1
  br i1 %5, label %6, label %7

6:                                                ; preds = %1
  store i64 1, i64* %2, align 8
  br label %19

7:                                                ; preds = %1
  %8 = load i64, i64* %3, align 8
  %9 = icmp eq i64 %8, 2
  br i1 %9, label %10, label %11

10:                                               ; preds = %7
  store i64 1, i64* %2, align 8
  br label %19

11:                                               ; preds = %7
  %12 = load i64, i64* %3, align 8
  %13 = sub nsw i64 %12, 1
  %14 = call i64 @fib(i64 %13)
  %15 = load i64, i64* %3, align 8
  %16 = sub nsw i64 %15, 2
  %17 = call i64 @fib(i64 %16)
  %18 = add nsw i64 %14, %17
  store i64 %18, i64* %2, align 8
  br label %19

19:                                               ; preds = %11, %10, %6
  %20 = load i64, i64* %2, align 8
  ret i64 %20
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @aaa(double %0) #0 {
  %2 = alloca i32, align 4
  %3 = alloca double, align 8
  store double %0, double* %3, align 8
  %4 = load double, double* %3, align 8
  %5 = fcmp oeq double %4, 3.000000e+00
  br i1 %5, label %6, label %7

6:                                                ; preds = %1
  store i32 1, i32* %2, align 4
  br label %8

7:                                                ; preds = %1
  store i32 2, i32* %2, align 4
  br label %8

8:                                                ; preds = %7, %6
  %9 = load i32, i32* %2, align 4
  ret i32 %9
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @factorial(i32 %0, i32 %1) #0 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  store i32 %0, i32* %4, align 4
  store i32 %1, i32* %5, align 4
  %6 = load i32, i32* %4, align 4
  %7 = icmp eq i32 %6, 0
  br i1 %7, label %8, label %10

8:                                                ; preds = %2
  %9 = load i32, i32* %5, align 4
  store i32 %9, i32* %3, align 4
  br label %17

10:                                               ; preds = %2
  %11 = load i32, i32* %4, align 4
  %12 = sub nsw i32 %11, 1
  %13 = load i32, i32* %5, align 4
  %14 = load i32, i32* %4, align 4
  %15 = mul nsw i32 %13, %14
  %16 = call i32 @factorial(i32 %12, i32 %15)
  store i32 %16, i32* %3, align 4
  br label %17

17:                                               ; preds = %10, %8
  %18 = load i32, i32* %3, align 4
  ret i32 %18
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0), i8** @zomo, align 8
  %2 = load i8*, i8** @zomo, align 8
  %3 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str.1, i64 0, i64 0), i8* %2)
  %4 = call i64 @fib(i64 40)
  %5 = trunc i64 %4 to i32
  ret i32 %5
}

declare i32 @printf(i8*, ...) #1

attributes #0 = { noinline nounwind optnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #1 = { "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 12, i32 3]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 13.1.6 (clang-1316.0.21.2.5)"}
