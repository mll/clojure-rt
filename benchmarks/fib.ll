; ModuleID = 'fib.c'
source_filename = "fib.c"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx12.0.0"

; Function Attrs: nofree nosync nounwind readnone ssp uwtable
define i64 @fib(i64 %0) local_unnamed_addr #0 {
  %2 = add i64 %0, -1
  %3 = icmp ult i64 %2, 2
  br i1 %3, label %15, label %4

4:                                                ; preds = %1, %4
  %5 = phi i64 [ %11, %4 ], [ %2, %1 ]
  %6 = phi i64 [ %9, %4 ], [ %0, %1 ]
  %7 = phi i64 [ %10, %4 ], [ 0, %1 ]
  %8 = tail call i64 @fib(i64 %5)
  %9 = add nsw i64 %6, -2
  %10 = add nsw i64 %8, %7
  %11 = add i64 %6, -3
  %12 = icmp ult i64 %11, 2
  br i1 %12, label %13, label %4

13:                                               ; preds = %4
  %14 = add i64 %10, 1
  br label %15

15:                                               ; preds = %13, %1
  %16 = phi i64 [ 1, %1 ], [ %14, %13 ]
  ret i64 %16
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone ssp uwtable willreturn
define i32 @aaa(double %0) local_unnamed_addr #1 {
  %2 = fcmp fast oeq double %0, 3.000000e+00
  %3 = select i1 %2, i32 1, i32 2
  ret i32 %3
}

; Function Attrs: nofree nosync nounwind readnone ssp uwtable
define i32 @factorial(i32 %0, i32 %1) local_unnamed_addr #0 {
  %3 = icmp eq i32 %0, 0
  br i1 %3, label %76, label %4

4:                                                ; preds = %2
  %5 = icmp ult i32 %0, 8
  br i1 %5, label %67, label %6

6:                                                ; preds = %4
  %7 = and i32 %0, -8
  %8 = and i32 %0, 7
  %9 = insertelement <4 x i32> <i32 poison, i32 1, i32 1, i32 1>, i32 %1, i32 0
  %10 = insertelement <4 x i32> poison, i32 %0, i32 0
  %11 = shufflevector <4 x i32> %10, <4 x i32> poison, <4 x i32> zeroinitializer
  %12 = add <4 x i32> %11, <i32 0, i32 -1, i32 -2, i32 -3>
  %13 = add i32 %7, -8
  %14 = lshr exact i32 %13, 3
  %15 = add nuw nsw i32 %14, 1
  %16 = and i32 %15, 3
  %17 = icmp ult i32 %13, 24
  br i1 %17, label %43, label %18

18:                                               ; preds = %6
  %19 = and i32 %15, 1073741820
  br label %20

20:                                               ; preds = %20, %18
  %21 = phi <4 x i32> [ %9, %18 ], [ %38, %20 ]
  %22 = phi <4 x i32> [ <i32 1, i32 1, i32 1, i32 1>, %18 ], [ %39, %20 ]
  %23 = phi <4 x i32> [ %12, %18 ], [ %40, %20 ]
  %24 = phi i32 [ %19, %18 ], [ %41, %20 ]
  %25 = add <4 x i32> %23, <i32 -4, i32 -4, i32 -4, i32 -4>
  %26 = mul <4 x i32> %21, %23
  %27 = mul <4 x i32> %22, %25
  %28 = add <4 x i32> %23, <i32 -8, i32 -8, i32 -8, i32 -8>
  %29 = add <4 x i32> %23, <i32 -12, i32 -12, i32 -12, i32 -12>
  %30 = mul <4 x i32> %26, %28
  %31 = mul <4 x i32> %27, %29
  %32 = add <4 x i32> %23, <i32 -16, i32 -16, i32 -16, i32 -16>
  %33 = add <4 x i32> %23, <i32 -20, i32 -20, i32 -20, i32 -20>
  %34 = mul <4 x i32> %30, %32
  %35 = mul <4 x i32> %31, %33
  %36 = add <4 x i32> %23, <i32 -24, i32 -24, i32 -24, i32 -24>
  %37 = add <4 x i32> %23, <i32 -28, i32 -28, i32 -28, i32 -28>
  %38 = mul <4 x i32> %34, %36
  %39 = mul <4 x i32> %35, %37
  %40 = add <4 x i32> %23, <i32 -32, i32 -32, i32 -32, i32 -32>
  %41 = add i32 %24, -4
  %42 = icmp eq i32 %41, 0
  br i1 %42, label %43, label %20, !llvm.loop !6

43:                                               ; preds = %20, %6
  %44 = phi <4 x i32> [ undef, %6 ], [ %38, %20 ]
  %45 = phi <4 x i32> [ undef, %6 ], [ %39, %20 ]
  %46 = phi <4 x i32> [ %9, %6 ], [ %38, %20 ]
  %47 = phi <4 x i32> [ <i32 1, i32 1, i32 1, i32 1>, %6 ], [ %39, %20 ]
  %48 = phi <4 x i32> [ %12, %6 ], [ %40, %20 ]
  %49 = icmp eq i32 %16, 0
  br i1 %49, label %61, label %50

50:                                               ; preds = %43, %50
  %51 = phi <4 x i32> [ %56, %50 ], [ %46, %43 ]
  %52 = phi <4 x i32> [ %57, %50 ], [ %47, %43 ]
  %53 = phi <4 x i32> [ %58, %50 ], [ %48, %43 ]
  %54 = phi i32 [ %59, %50 ], [ %16, %43 ]
  %55 = add <4 x i32> %53, <i32 -4, i32 -4, i32 -4, i32 -4>
  %56 = mul <4 x i32> %51, %53
  %57 = mul <4 x i32> %52, %55
  %58 = add <4 x i32> %53, <i32 -8, i32 -8, i32 -8, i32 -8>
  %59 = add i32 %54, -1
  %60 = icmp eq i32 %59, 0
  br i1 %60, label %61, label %50, !llvm.loop !8

61:                                               ; preds = %50, %43
  %62 = phi <4 x i32> [ %44, %43 ], [ %56, %50 ]
  %63 = phi <4 x i32> [ %45, %43 ], [ %57, %50 ]
  %64 = mul <4 x i32> %63, %62
  %65 = call i32 @llvm.vector.reduce.mul.v4i32(<4 x i32> %64)
  %66 = icmp eq i32 %7, %0
  br i1 %66, label %76, label %67

67:                                               ; preds = %4, %61
  %68 = phi i32 [ %1, %4 ], [ %65, %61 ]
  %69 = phi i32 [ %0, %4 ], [ %8, %61 ]
  br label %70

70:                                               ; preds = %67, %70
  %71 = phi i32 [ %74, %70 ], [ %68, %67 ]
  %72 = phi i32 [ %73, %70 ], [ %69, %67 ]
  %73 = add nsw i32 %72, -1
  %74 = mul nsw i32 %71, %72
  %75 = icmp eq i32 %73, 0
  br i1 %75, label %76, label %70, !llvm.loop !10

76:                                               ; preds = %70, %61, %2
  %77 = phi i32 [ %1, %2 ], [ %65, %61 ], [ %74, %70 ]
  ret i32 %77
}

; Function Attrs: nofree nosync nounwind readnone ssp uwtable
define i32 @main() local_unnamed_addr #0 {
  %1 = tail call i64 @fib(i64 40)
  br label %2

2:                                                ; preds = %2, %0
  %3 = phi i32 [ 0, %0 ], [ %26, %2 ]
  %4 = phi <4 x i32> [ <i32 4, i32 1, i32 1, i32 1>, %0 ], [ %24, %2 ]
  %5 = phi <4 x i32> [ <i32 1, i32 1, i32 1, i32 1>, %0 ], [ %25, %2 ]
  %6 = phi <4 x i32> [ <i32 800, i32 799, i32 798, i32 797>, %0 ], [ %27, %2 ]
  %7 = add <4 x i32> %6, <i32 -4, i32 -4, i32 -4, i32 -4>
  %8 = mul <4 x i32> %6, %4
  %9 = mul <4 x i32> %7, %5
  %10 = add <4 x i32> %6, <i32 -8, i32 -8, i32 -8, i32 -8>
  %11 = add <4 x i32> %6, <i32 -12, i32 -12, i32 -12, i32 -12>
  %12 = mul <4 x i32> %10, %8
  %13 = mul <4 x i32> %11, %9
  %14 = add <4 x i32> %6, <i32 -16, i32 -16, i32 -16, i32 -16>
  %15 = add <4 x i32> %6, <i32 -20, i32 -20, i32 -20, i32 -20>
  %16 = mul <4 x i32> %14, %12
  %17 = mul <4 x i32> %15, %13
  %18 = add <4 x i32> %6, <i32 -24, i32 -24, i32 -24, i32 -24>
  %19 = add <4 x i32> %6, <i32 -28, i32 -28, i32 -28, i32 -28>
  %20 = mul <4 x i32> %18, %16
  %21 = mul <4 x i32> %19, %17
  %22 = add <4 x i32> %6, <i32 -32, i32 -32, i32 -32, i32 -32>
  %23 = add <4 x i32> %6, <i32 -36, i32 -36, i32 -36, i32 -36>
  %24 = mul <4 x i32> %22, %20
  %25 = mul <4 x i32> %23, %21
  %26 = add nuw nsw i32 %3, 40
  %27 = add <4 x i32> %6, <i32 -40, i32 -40, i32 -40, i32 -40>
  %28 = icmp eq i32 %26, 800
  br i1 %28, label %29, label %2, !llvm.loop !12

29:                                               ; preds = %2
  %30 = mul <4 x i32> %25, %24
  %31 = call i32 @llvm.vector.reduce.mul.v4i32(<4 x i32> %30)
  %32 = trunc i64 %1 to i32
  %33 = add i32 %31, %32
  ret i32 %33
}

; Function Attrs: nofree nosync nounwind readnone willreturn
declare i32 @llvm.vector.reduce.mul.v4i32(<4 x i32>) #2

attributes #0 = { nofree nosync nounwind readnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #1 = { mustprogress nofree norecurse nosync nounwind readnone ssp uwtable willreturn "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #2 = { nofree nosync nounwind readnone willreturn }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 12, i32 3]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 13.1.6 (clang-1316.0.21.2.5)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.isvectorized", i32 1}
!8 = distinct !{!8, !9}
!9 = !{!"llvm.loop.unroll.disable"}
!10 = distinct !{!10, !11, !7}
!11 = !{!"llvm.loop.unroll.runtime.disable"}
!12 = distinct !{!12, !7}
