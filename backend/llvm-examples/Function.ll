; ModuleID = 'Function.c'
source_filename = "Function.c"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx13.0.0"

%struct.Object = type { i32, i64 }
%struct.Function = type { i64, i64, i64, i8, i8, [0 x %struct.FunctionMethod] }
%struct.FunctionMethod = type { i8, i64, i64, i64, ptr, ptr, [3 x %struct.InvokationCache] }
%struct.InvokationCache = type { [3 x i64], i64, i8, ptr }
%struct.__va_list_tag = type { i32, i32, ptr, ptr }

@.str = private unnamed_addr constant [4 x i8] c"fn_\00", align 1
@.str.1 = private unnamed_addr constant [49 x i8] c"Function with :once meta executed more than once\00", align 1
@__func__.Function_cleanupOnce = private unnamed_addr constant [21 x i8] c"Function_cleanupOnce\00", align 1
@.str.2 = private unnamed_addr constant [11 x i8] c"Function.c\00", align 1
@.str.3 = private unnamed_addr constant [70 x i8] c"!self->executed && \22Function with :once meta executed more than once\22\00", align 1

; Function Attrs: noinline nounwind optnone ssp uwtable
define ptr @Function_create(i64 noundef %0, i64 noundef %1, i64 noundef %2, i8 noundef zeroext %3) #0 {
  %5 = alloca i64, align 8
  %6 = alloca i64, align 8
  %7 = alloca i64, align 8
  %8 = alloca i8, align 1
  %9 = alloca i64, align 8
  %10 = alloca ptr, align 8
  %11 = alloca ptr, align 8
  store i64 %0, ptr %5, align 8
  store i64 %1, ptr %6, align 8
  store i64 %2, ptr %7, align 8
  store i8 %3, ptr %8, align 1
  %12 = load i64, ptr %5, align 8
  %13 = mul i64 %12, 192
  %14 = add i64 48, %13
  store i64 %14, ptr %9, align 8
  %15 = load i64, ptr %9, align 8
  %16 = call ptr @allocate(i64 noundef %15)
  store ptr %16, ptr %10, align 8
  %17 = load ptr, ptr %10, align 8
  %18 = load i64, ptr %9, align 8
  %19 = load ptr, ptr %10, align 8
  %20 = call i64 @llvm.objectsize.i64.p0(ptr %19, i1 false, i1 true, i1 false)
  %21 = call ptr @__memset_chk(ptr noundef %17, i32 noundef 0, i64 noundef %18, i64 noundef %20) #6
  %22 = load ptr, ptr %10, align 8
  %23 = getelementptr inbounds %struct.Object, ptr %22, i64 1
  store ptr %23, ptr %11, align 8
  %24 = load i64, ptr %5, align 8
  %25 = load ptr, ptr %11, align 8
  %26 = getelementptr inbounds %struct.Function, ptr %25, i32 0, i32 1
  store i64 %24, ptr %26, align 8
  %27 = load i64, ptr %7, align 8
  %28 = load ptr, ptr %11, align 8
  %29 = getelementptr inbounds %struct.Function, ptr %28, i32 0, i32 2
  store i64 %27, ptr %29, align 8
  %30 = load i64, ptr %6, align 8
  %31 = load ptr, ptr %11, align 8
  %32 = getelementptr inbounds %struct.Function, ptr %31, i32 0, i32 0
  store i64 %30, ptr %32, align 8
  %33 = load i8, ptr %8, align 1
  %34 = load ptr, ptr %11, align 8
  %35 = getelementptr inbounds %struct.Function, ptr %34, i32 0, i32 3
  store i8 %33, ptr %35, align 8
  %36 = load ptr, ptr %11, align 8
  %37 = getelementptr inbounds %struct.Function, ptr %36, i32 0, i32 4
  store i8 0, ptr %37, align 1
  %38 = load ptr, ptr %10, align 8
  call void @Object_create(ptr noundef %38, i32 noundef 12)
  %39 = load ptr, ptr %11, align 8
  ret ptr %39
}

declare ptr @allocate(i64 noundef) #1

; Function Attrs: nounwind
declare ptr @__memset_chk(ptr noundef, i32 noundef, i64 noundef, i64 noundef) #2

; Function Attrs: nocallback nofree nosync nounwind readnone speculatable willreturn
declare i64 @llvm.objectsize.i64.p0(ptr, i1 immarg, i1 immarg, i1 immarg) #3

declare void @Object_create(ptr noundef, i32 noundef) #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @Function_fillMethod(ptr noundef %0, i64 noundef %1, i64 noundef %2, i64 noundef %3, i8 noundef zeroext %4, ptr noundef %5, i64 noundef %6, ...) #0 {
  %8 = alloca ptr, align 8
  %9 = alloca i64, align 8
  %10 = alloca i64, align 8
  %11 = alloca i64, align 8
  %12 = alloca i8, align 1
  %13 = alloca ptr, align 8
  %14 = alloca i64, align 8
  %15 = alloca ptr, align 8
  %16 = alloca [1 x %struct.__va_list_tag], align 16
  %17 = alloca i32, align 4
  %18 = alloca ptr, align 8
  store ptr %0, ptr %8, align 8
  store i64 %1, ptr %9, align 8
  store i64 %2, ptr %10, align 8
  store i64 %3, ptr %11, align 8
  store i8 %4, ptr %12, align 1
  store ptr %5, ptr %13, align 8
  store i64 %6, ptr %14, align 8
  %19 = load ptr, ptr %8, align 8
  %20 = getelementptr inbounds %struct.Function, ptr %19, i32 0, i32 5
  %21 = getelementptr inbounds [0 x %struct.FunctionMethod], ptr %20, i64 0, i64 0
  %22 = load i64, ptr %9, align 8
  %23 = getelementptr inbounds %struct.FunctionMethod, ptr %21, i64 %22
  store ptr %23, ptr %15, align 8
  %24 = load i64, ptr %11, align 8
  %25 = load ptr, ptr %15, align 8
  %26 = getelementptr inbounds %struct.FunctionMethod, ptr %25, i32 0, i32 1
  store i64 %24, ptr %26, align 8
  %27 = load i8, ptr %12, align 1
  %28 = zext i8 %27 to i64
  %29 = load ptr, ptr %15, align 8
  %30 = getelementptr inbounds %struct.FunctionMethod, ptr %29, i32 0, i32 2
  store i64 %28, ptr %30, align 8
  %31 = load ptr, ptr %13, align 8
  %32 = load ptr, ptr %15, align 8
  %33 = getelementptr inbounds %struct.FunctionMethod, ptr %32, i32 0, i32 4
  store ptr %31, ptr %33, align 8
  %34 = load i64, ptr %10, align 8
  %35 = trunc i64 %34 to i8
  %36 = load ptr, ptr %15, align 8
  %37 = getelementptr inbounds %struct.FunctionMethod, ptr %36, i32 0, i32 0
  store i8 %35, ptr %37, align 8
  %38 = load i64, ptr %14, align 8
  %39 = load ptr, ptr %15, align 8
  %40 = getelementptr inbounds %struct.FunctionMethod, ptr %39, i32 0, i32 3
  store i64 %38, ptr %40, align 8
  %41 = load i64, ptr %14, align 8
  %42 = icmp sgt i64 %41, 0
  br i1 %42, label %43, label %49

43:                                               ; preds = %7
  %44 = load i64, ptr %14, align 8
  %45 = mul i64 %44, 8
  %46 = call ptr @allocate(i64 noundef %45)
  %47 = load ptr, ptr %15, align 8
  %48 = getelementptr inbounds %struct.FunctionMethod, ptr %47, i32 0, i32 5
  store ptr %46, ptr %48, align 8
  br label %49

49:                                               ; preds = %43, %7
  %50 = getelementptr inbounds [1 x %struct.__va_list_tag], ptr %16, i64 0, i64 0
  call void @llvm.va_start(ptr %50)
  store i32 0, ptr %17, align 4
  br label %51

51:                                               ; preds = %80, %49
  %52 = load i32, ptr %17, align 4
  %53 = sext i32 %52 to i64
  %54 = load i64, ptr %14, align 8
  %55 = icmp slt i64 %53, %54
  br i1 %55, label %56, label %83

56:                                               ; preds = %51
  %57 = getelementptr inbounds [1 x %struct.__va_list_tag], ptr %16, i64 0, i64 0
  %58 = getelementptr inbounds %struct.__va_list_tag, ptr %57, i32 0, i32 0
  %59 = load i32, ptr %58, align 16
  %60 = icmp ule i32 %59, 40
  br i1 %60, label %61, label %66

61:                                               ; preds = %56
  %62 = getelementptr inbounds %struct.__va_list_tag, ptr %57, i32 0, i32 3
  %63 = load ptr, ptr %62, align 16
  %64 = getelementptr i8, ptr %63, i32 %59
  %65 = add i32 %59, 8
  store i32 %65, ptr %58, align 16
  br label %70

66:                                               ; preds = %56
  %67 = getelementptr inbounds %struct.__va_list_tag, ptr %57, i32 0, i32 2
  %68 = load ptr, ptr %67, align 8
  %69 = getelementptr i8, ptr %68, i32 8
  store ptr %69, ptr %67, align 8
  br label %70

70:                                               ; preds = %66, %61
  %71 = phi ptr [ %64, %61 ], [ %68, %66 ]
  %72 = load ptr, ptr %71, align 8
  store ptr %72, ptr %18, align 8
  %73 = load ptr, ptr %18, align 8
  %74 = load ptr, ptr %15, align 8
  %75 = getelementptr inbounds %struct.FunctionMethod, ptr %74, i32 0, i32 5
  %76 = load ptr, ptr %75, align 8
  %77 = load i32, ptr %17, align 4
  %78 = sext i32 %77 to i64
  %79 = getelementptr inbounds ptr, ptr %76, i64 %78
  store ptr %73, ptr %79, align 8
  br label %80

80:                                               ; preds = %70
  %81 = load i32, ptr %17, align 4
  %82 = add nsw i32 %81, 1
  store i32 %82, ptr %17, align 4
  br label %51, !llvm.loop !6

83:                                               ; preds = %51
  %84 = getelementptr inbounds [1 x %struct.__va_list_tag], ptr %16, i64 0, i64 0
  call void @llvm.va_end(ptr %84)
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn
declare void @llvm.va_start(ptr) #4

; Function Attrs: nocallback nofree nosync nounwind willreturn
declare void @llvm.va_end(ptr) #4

; Function Attrs: noinline nounwind optnone ssp uwtable
define zeroext i8 @Function_equals(ptr noundef %0, ptr noundef %1) #0 {
  %3 = alloca ptr, align 8
  %4 = alloca ptr, align 8
  store ptr %0, ptr %3, align 8
  store ptr %1, ptr %4, align 8
  %5 = load ptr, ptr %3, align 8
  %6 = getelementptr inbounds %struct.Function, ptr %5, i32 0, i32 0
  %7 = load i64, ptr %6, align 8
  %8 = load ptr, ptr %4, align 8
  %9 = getelementptr inbounds %struct.Function, ptr %8, i32 0, i32 0
  %10 = load i64, ptr %9, align 8
  %11 = icmp eq i64 %7, %10
  %12 = zext i1 %11 to i32
  %13 = trunc i32 %12 to i8
  ret i8 %13
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i64 @Function_hash(ptr noundef %0) #0 {
  %2 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %3 = load ptr, ptr %2, align 8
  %4 = getelementptr inbounds %struct.Function, ptr %3, i32 0, i32 0
  %5 = load i64, ptr %4, align 8
  %6 = call i64 @avalanche_64(i64 noundef %5)
  ret i64 %6
}

declare i64 @avalanche_64(i64 noundef) #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define ptr @Function_toString(ptr noundef %0) #0 {
  %2 = alloca ptr, align 8
  %3 = alloca ptr, align 8
  store ptr %0, ptr %2, align 8
  %4 = load ptr, ptr %2, align 8
  %5 = getelementptr inbounds %struct.Function, ptr %4, i32 0, i32 0
  %6 = load i64, ptr %5, align 8
  %7 = call ptr @Integer_create(i64 noundef %6)
  store ptr %7, ptr %3, align 8
  %8 = load ptr, ptr %2, align 8
  %9 = call zeroext i8 @release(ptr noundef %8)
  %10 = call ptr @String_createStatic(ptr noundef @.str)
  %11 = load ptr, ptr %3, align 8
  %12 = call ptr @toString(ptr noundef %11)
  %13 = call ptr @String_concat(ptr noundef %10, ptr noundef %12)
  ret ptr %13
}

declare ptr @Integer_create(i64 noundef) #1

declare zeroext i8 @release(ptr noundef) #1

declare ptr @String_concat(ptr noundef, ptr noundef) #1

declare ptr @String_createStatic(ptr noundef) #1

declare ptr @toString(ptr noundef) #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @Function_cleanupOnce(ptr noundef %0) #0 {
  %2 = alloca ptr, align 8
  %3 = alloca i32, align 4
  %4 = alloca ptr, align 8
  %5 = alloca i32, align 4
  store ptr %0, ptr %2, align 8
  %6 = load ptr, ptr %2, align 8
  %7 = getelementptr inbounds %struct.Function, ptr %6, i32 0, i32 4
  %8 = load i8, ptr %7, align 1
  %9 = icmp ne i8 %8, 0
  br i1 %9, label %11, label %10

10:                                               ; preds = %1
  br label %11

11:                                               ; preds = %10, %1
  %12 = phi i1 [ false, %1 ], [ true, %10 ]
  %13 = xor i1 %12, true
  %14 = zext i1 %13 to i32
  %15 = sext i32 %14 to i64
  %16 = icmp ne i64 %15, 0
  br i1 %16, label %17, label %19

17:                                               ; preds = %11
  call void @__assert_rtn(ptr noundef @__func__.Function_cleanupOnce, ptr noundef @.str.2, i32 noundef 60, ptr noundef @.str.3) #7
  unreachable

18:                                               ; No predecessors!
  br label %20

19:                                               ; preds = %11
  br label %20

20:                                               ; preds = %19, %18
  %21 = load ptr, ptr %2, align 8
  %22 = getelementptr inbounds %struct.Function, ptr %21, i32 0, i32 4
  store i8 1, ptr %22, align 1
  store i32 0, ptr %3, align 4
  br label %23

23:                                               ; preds = %65, %20
  %24 = load i32, ptr %3, align 4
  %25 = sext i32 %24 to i64
  %26 = load ptr, ptr %2, align 8
  %27 = getelementptr inbounds %struct.Function, ptr %26, i32 0, i32 1
  %28 = load i64, ptr %27, align 8
  %29 = icmp ult i64 %25, %28
  br i1 %29, label %30, label %68

30:                                               ; preds = %23
  %31 = load ptr, ptr %2, align 8
  %32 = getelementptr inbounds %struct.Function, ptr %31, i32 0, i32 5
  %33 = load i32, ptr %3, align 4
  %34 = sext i32 %33 to i64
  %35 = getelementptr inbounds [0 x %struct.FunctionMethod], ptr %32, i64 0, i64 %34
  store ptr %35, ptr %4, align 8
  store i32 0, ptr %5, align 4
  br label %36

36:                                               ; preds = %52, %30
  %37 = load i32, ptr %5, align 4
  %38 = sext i32 %37 to i64
  %39 = load ptr, ptr %4, align 8
  %40 = getelementptr inbounds %struct.FunctionMethod, ptr %39, i32 0, i32 3
  %41 = load i64, ptr %40, align 8
  %42 = icmp ult i64 %38, %41
  br i1 %42, label %43, label %55

43:                                               ; preds = %36
  %44 = load ptr, ptr %4, align 8
  %45 = getelementptr inbounds %struct.FunctionMethod, ptr %44, i32 0, i32 5
  %46 = load ptr, ptr %45, align 8
  %47 = load i32, ptr %5, align 4
  %48 = sext i32 %47 to i64
  %49 = getelementptr inbounds ptr, ptr %46, i64 %48
  %50 = load ptr, ptr %49, align 8
  %51 = call zeroext i8 @release(ptr noundef %50)
  br label %52

52:                                               ; preds = %43
  %53 = load i32, ptr %5, align 4
  %54 = add nsw i32 %53, 1
  store i32 %54, ptr %5, align 4
  br label %36, !llvm.loop !8

55:                                               ; preds = %36
  %56 = load ptr, ptr %4, align 8
  %57 = getelementptr inbounds %struct.FunctionMethod, ptr %56, i32 0, i32 3
  %58 = load i64, ptr %57, align 8
  %59 = icmp ne i64 %58, 0
  br i1 %59, label %60, label %64

60:                                               ; preds = %55
  %61 = load ptr, ptr %4, align 8
  %62 = getelementptr inbounds %struct.FunctionMethod, ptr %61, i32 0, i32 5
  %63 = load ptr, ptr %62, align 8
  call void @deallocate(ptr noundef %63)
  br label %64

64:                                               ; preds = %60, %55
  br label %65

65:                                               ; preds = %64
  %66 = load i32, ptr %3, align 4
  %67 = add nsw i32 %66, 1
  store i32 %67, ptr %3, align 4
  br label %23, !llvm.loop !9

68:                                               ; preds = %23
  ret void
}

; Function Attrs: cold noreturn
declare void @__assert_rtn(ptr noundef, ptr noundef, i32 noundef, ptr noundef) #5

declare void @deallocate(ptr noundef) #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @Function_destroy(ptr noundef %0) #0 {
  %2 = alloca ptr, align 8
  %3 = alloca i32, align 4
  %4 = alloca ptr, align 8
  %5 = alloca i32, align 4
  store ptr %0, ptr %2, align 8
  %6 = load ptr, ptr %2, align 8
  %7 = getelementptr inbounds %struct.Function, ptr %6, i32 0, i32 3
  %8 = load i8, ptr %7, align 8
  %9 = zext i8 %8 to i32
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %11, label %18

11:                                               ; preds = %1
  %12 = load ptr, ptr %2, align 8
  %13 = getelementptr inbounds %struct.Function, ptr %12, i32 0, i32 4
  %14 = load i8, ptr %13, align 1
  %15 = zext i8 %14 to i32
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %17, label %18

17:                                               ; preds = %11
  br label %64

18:                                               ; preds = %11, %1
  store i32 0, ptr %3, align 4
  br label %19

19:                                               ; preds = %61, %18
  %20 = load i32, ptr %3, align 4
  %21 = sext i32 %20 to i64
  %22 = load ptr, ptr %2, align 8
  %23 = getelementptr inbounds %struct.Function, ptr %22, i32 0, i32 1
  %24 = load i64, ptr %23, align 8
  %25 = icmp ult i64 %21, %24
  br i1 %25, label %26, label %64

26:                                               ; preds = %19
  %27 = load ptr, ptr %2, align 8
  %28 = getelementptr inbounds %struct.Function, ptr %27, i32 0, i32 5
  %29 = load i32, ptr %3, align 4
  %30 = sext i32 %29 to i64
  %31 = getelementptr inbounds [0 x %struct.FunctionMethod], ptr %28, i64 0, i64 %30
  store ptr %31, ptr %4, align 8
  store i32 0, ptr %5, align 4
  br label %32

32:                                               ; preds = %48, %26
  %33 = load i32, ptr %5, align 4
  %34 = sext i32 %33 to i64
  %35 = load ptr, ptr %4, align 8
  %36 = getelementptr inbounds %struct.FunctionMethod, ptr %35, i32 0, i32 3
  %37 = load i64, ptr %36, align 8
  %38 = icmp ult i64 %34, %37
  br i1 %38, label %39, label %51

39:                                               ; preds = %32
  %40 = load ptr, ptr %4, align 8
  %41 = getelementptr inbounds %struct.FunctionMethod, ptr %40, i32 0, i32 5
  %42 = load ptr, ptr %41, align 8
  %43 = load i32, ptr %5, align 4
  %44 = sext i32 %43 to i64
  %45 = getelementptr inbounds ptr, ptr %42, i64 %44
  %46 = load ptr, ptr %45, align 8
  %47 = call zeroext i8 @release(ptr noundef %46)
  br label %48

48:                                               ; preds = %39
  %49 = load i32, ptr %5, align 4
  %50 = add nsw i32 %49, 1
  store i32 %50, ptr %5, align 4
  br label %32, !llvm.loop !10

51:                                               ; preds = %32
  %52 = load ptr, ptr %4, align 8
  %53 = getelementptr inbounds %struct.FunctionMethod, ptr %52, i32 0, i32 3
  %54 = load i64, ptr %53, align 8
  %55 = icmp ne i64 %54, 0
  br i1 %55, label %56, label %60

56:                                               ; preds = %51
  %57 = load ptr, ptr %4, align 8
  %58 = getelementptr inbounds %struct.FunctionMethod, ptr %57, i32 0, i32 5
  %59 = load ptr, ptr %58, align 8
  call void @deallocate(ptr noundef %59)
  br label %60

60:                                               ; preds = %56, %51
  br label %61

61:                                               ; preds = %60
  %62 = load i32, ptr %3, align 4
  %63 = add nsw i32 %62, 1
  store i32 %63, ptr %3, align 4
  br label %19, !llvm.loop !11

64:                                               ; preds = %17, %19
  ret void
}

attributes #0 = { noinline nounwind optnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #1 = { "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #2 = { nounwind "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #3 = { nocallback nofree nosync nounwind readnone speculatable willreturn }
attributes #4 = { nocallback nofree nosync nounwind willreturn }
attributes #5 = { cold noreturn "darwin-stkchk-strong-link" "disable-tail-calls"="true" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #6 = { nounwind }
attributes #7 = { cold noreturn }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 14, i32 2]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 8, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 15.0.0 (clang-1500.1.0.2.5)"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.mustprogress"}
!8 = distinct !{!8, !7}
!9 = distinct !{!9, !7}
!10 = distinct !{!10, !7}
!11 = distinct !{!11, !7}
