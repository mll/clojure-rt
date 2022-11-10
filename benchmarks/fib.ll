; ModuleID = 'fib.c'
source_filename = "fib.c"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx12.0.0"

@.str = private unnamed_addr constant [5 x i8] c"%lld\00", align 1

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

; Function Attrs: nofree nounwind ssp uwtable
define i32 @main() local_unnamed_addr #1 {
  %1 = tail call i64 @fib(i64 42)
  %2 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([5 x i8], [5 x i8]* @.str, i64 0, i64 0), i64 %1)
  ret i32 0
}

; Function Attrs: nofree nounwind
declare noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #2

attributes #0 = { nofree nosync nounwind readnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #1 = { nofree nounwind ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #2 = { nofree nounwind "darwin-stkchk-strong-link" "frame-pointer"="all" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 12, i32 3]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 13.1.6 (clang-1316.0.21.2.5)"}
