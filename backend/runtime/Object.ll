; ModuleID = 'Object.c'
source_filename = "Object.c"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx12.0.0"

%struct.Object = type { i8, i64 }
%struct.Integer = type { i64 }
%struct.String = type { i8*, i32 }
%struct.PersistentList = type { %struct.Object*, %struct.PersistentList*, i64 }
%struct.PersistentVector = type { i64, i64, %struct.PersistentVectorNode*, %struct.PersistentVectorNode* }
%struct.PersistentVectorNode = type { i8, i64, [0 x %struct.Object*] }
%struct.Double = type { double }
%struct.Boolean = type { i8 }
%struct.Nil = type {}

@__func__.Object_release_internal = private unnamed_addr constant [24 x i8] c"Object_release_internal\00", align 1
@.str = private unnamed_addr constant [9 x i8] c"Object.h\00", align 1
@.str.1 = private unnamed_addr constant [6 x i8] c"FALSE\00", align 1
@__func__.equals = private unnamed_addr constant [7 x i8] c"equals\00", align 1
@__func__.hash = private unnamed_addr constant [5 x i8] c"hash\00", align 1
@__func__.toString = private unnamed_addr constant [9 x i8] c"toString\00", align 1

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @initialise_memory() #0 {
  call void (...) @PersistentVector_initialise()
  call void (...) @Nil_initialise()
  ret void
}

declare void @PersistentVector_initialise(...) #1

declare void @Nil_initialise(...) #1

; Function Attrs: noinline nounwind optnone ssp uwtable
define i8* @allocate(i64 %0) #0 {
  %2 = alloca i64, align 8
  store i64 %0, i64* %2, align 8
  %3 = load i64, i64* %2, align 8
  %4 = call i8* @malloc(i64 %3) #4
  ret i8* %4
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @deallocate(i8* %0) #0 {
  %2 = alloca i8*, align 8
  store i8* %0, i8** %2, align 8
  %3 = load i8*, i8** %2, align 8
  call void @free(i8* %3)
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i8* @data(%struct.Object* %0) #0 {
  %2 = alloca %struct.Object*, align 8
  store %struct.Object* %0, %struct.Object** %2, align 8
  %3 = load %struct.Object*, %struct.Object** %2, align 8
  %4 = getelementptr inbounds %struct.Object, %struct.Object* %3, i64 1
  %5 = bitcast %struct.Object* %4 to i8*
  ret i8* %5
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define %struct.Object* @super(i8* %0) #0 {
  %2 = alloca i8*, align 8
  store i8* %0, i8** %2, align 8
  %3 = load i8*, i8** %2, align 8
  %4 = getelementptr i8, i8* %3, i64 -16
  %5 = bitcast i8* %4 to %struct.Object*
  ret %struct.Object* %5
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @Object_create(%struct.Object* %0, i8 zeroext %1) #0 {
  %3 = alloca %struct.Object*, align 8
  %4 = alloca i8, align 1
  %5 = alloca i64, align 8
  store %struct.Object* %0, %struct.Object** %3, align 8
  store i8 %1, i8* %4, align 1
  %6 = load %struct.Object*, %struct.Object** %3, align 8
  %7 = getelementptr inbounds %struct.Object, %struct.Object* %6, i32 0, i32 1
  store i64 1, i64* %5, align 8
  %8 = load i64, i64* %5, align 8
  store atomic volatile i64 %8, i64* %7 monotonic, align 8
  %9 = load i8, i8* %4, align 1
  %10 = load %struct.Object*, %struct.Object** %3, align 8
  %11 = getelementptr inbounds %struct.Object, %struct.Object* %10, i32 0, i32 0
  store i8 %9, i8* %11, align 8
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @retain(i8* %0) #0 {
  %2 = alloca i8*, align 8
  store i8* %0, i8** %2, align 8
  %3 = load i8*, i8** %2, align 8
  %4 = call %struct.Object* @super(i8* %3)
  call void @Object_retain(%struct.Object* %4)
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define zeroext i8 @release(i8* %0) #0 {
  %2 = alloca i8*, align 8
  store i8* %0, i8** %2, align 8
  %3 = load i8*, i8** %2, align 8
  %4 = call %struct.Object* @super(i8* %3)
  %5 = call zeroext i8 @Object_release(%struct.Object* %4)
  ret i8 %5
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define void @Object_retain(%struct.Object* %0) #0 {
  %2 = alloca %struct.Object*, align 8
  %3 = alloca i64, align 8
  %4 = alloca i64, align 8
  store %struct.Object* %0, %struct.Object** %2, align 8
  %5 = load %struct.Object*, %struct.Object** %2, align 8
  %6 = getelementptr inbounds %struct.Object, %struct.Object* %5, i32 0, i32 1
  store i64 1, i64* %3, align 8
  %7 = load i64, i64* %3, align 8
  %8 = atomicrmw volatile add i64* %6, i64 %7 seq_cst, align 8
  store i64 %8, i64* %4, align 8
  %9 = load i64, i64* %4, align 8
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define zeroext i8 @Object_release(%struct.Object* %0) #0 {
  %2 = alloca %struct.Object*, align 8
  store %struct.Object* %0, %struct.Object** %2, align 8
  %3 = load %struct.Object*, %struct.Object** %2, align 8
  %4 = call zeroext i8 @Object_release_internal(%struct.Object* %3, i8 zeroext 1)
  ret i8 %4
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define zeroext i8 @Object_release_internal(%struct.Object* %0, i8 zeroext %1) #0 {
  %3 = alloca i8, align 1
  %4 = alloca %struct.Object*, align 8
  %5 = alloca i8, align 1
  %6 = alloca i64, align 8
  %7 = alloca i64, align 8
  store %struct.Object* %0, %struct.Object** %4, align 8
  store i8 %1, i8* %5, align 1
  %8 = load %struct.Object*, %struct.Object** %4, align 8
  %9 = getelementptr inbounds %struct.Object, %struct.Object* %8, i32 0, i32 1
  store i64 1, i64* %6, align 8
  %10 = load i64, i64* %6, align 8
  %11 = atomicrmw volatile sub i64* %9, i64 %10 seq_cst, align 8
  store i64 %11, i64* %7, align 8
  %12 = load i64, i64* %7, align 8
  %13 = icmp eq i64 %12, 1
  br i1 %13, label %14, label %58

14:                                               ; preds = %2
  %15 = load %struct.Object*, %struct.Object** %4, align 8
  %16 = getelementptr inbounds %struct.Object, %struct.Object* %15, i32 0, i32 0
  %17 = load i8, i8* %16, align 8
  %18 = zext i8 %17 to i32
  switch i32 %18, label %55 [
    i32 1, label %19
    i32 2, label %23
    i32 3, label %27
    i32 4, label %32
    i32 5, label %37
    i32 6, label %42
    i32 8, label %46
    i32 7, label %50
    i32 0, label %54
  ]

19:                                               ; preds = %14
  %20 = load %struct.Object*, %struct.Object** %4, align 8
  %21 = call i8* @data(%struct.Object* %20)
  %22 = bitcast i8* %21 to %struct.Integer*
  call void @Integer_destroy(%struct.Integer* %22)
  br label %55

23:                                               ; preds = %14
  %24 = load %struct.Object*, %struct.Object** %4, align 8
  %25 = call i8* @data(%struct.Object* %24)
  %26 = bitcast i8* %25 to %struct.String*
  call void @String_destroy(%struct.String* %26)
  br label %55

27:                                               ; preds = %14
  %28 = load %struct.Object*, %struct.Object** %4, align 8
  %29 = call i8* @data(%struct.Object* %28)
  %30 = bitcast i8* %29 to %struct.PersistentList*
  %31 = load i8, i8* %5, align 1
  call void @PersistentList_destroy(%struct.PersistentList* %30, i8 zeroext %31)
  br label %55

32:                                               ; preds = %14
  %33 = load %struct.Object*, %struct.Object** %4, align 8
  %34 = call i8* @data(%struct.Object* %33)
  %35 = bitcast i8* %34 to %struct.PersistentVector*
  %36 = load i8, i8* %5, align 1
  call void @PersistentVector_destroy(%struct.PersistentVector* %35, i8 zeroext %36)
  br label %55

37:                                               ; preds = %14
  %38 = load %struct.Object*, %struct.Object** %4, align 8
  %39 = call i8* @data(%struct.Object* %38)
  %40 = bitcast i8* %39 to %struct.PersistentVectorNode*
  %41 = load i8, i8* %5, align 1
  call void @PersistentVectorNode_destroy(%struct.PersistentVectorNode* %40, i8 zeroext %41)
  br label %55

42:                                               ; preds = %14
  %43 = load %struct.Object*, %struct.Object** %4, align 8
  %44 = call i8* @data(%struct.Object* %43)
  %45 = bitcast i8* %44 to %struct.Double*
  call void @Double_destroy(%struct.Double* %45)
  br label %55

46:                                               ; preds = %14
  %47 = load %struct.Object*, %struct.Object** %4, align 8
  %48 = call i8* @data(%struct.Object* %47)
  %49 = bitcast i8* %48 to %struct.Boolean*
  call void @Boolean_destroy(%struct.Boolean* %49)
  br label %55

50:                                               ; preds = %14
  %51 = load %struct.Object*, %struct.Object** %4, align 8
  %52 = call i8* @data(%struct.Object* %51)
  %53 = bitcast i8* %52 to %struct.Nil*
  call void @Nil_destroy(%struct.Nil* %53)
  br label %55

54:                                               ; preds = %14
  call void @__assert_rtn(i8* getelementptr inbounds ([24 x i8], [24 x i8]* @__func__.Object_release_internal, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str, i64 0, i64 0), i32 98, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.1, i64 0, i64 0)) #5
  unreachable

55:                                               ; preds = %14, %50, %46, %42, %37, %32, %27, %23, %19
  %56 = load %struct.Object*, %struct.Object** %4, align 8
  %57 = bitcast %struct.Object* %56 to i8*
  call void @deallocate(i8* %57)
  store i8 1, i8* %3, align 1
  br label %59

58:                                               ; preds = %2
  store i8 0, i8* %3, align 1
  br label %59

59:                                               ; preds = %58, %55
  %60 = load i8, i8* %3, align 1
  ret i8 %60
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define zeroext i8 @equals(%struct.Object* %0, %struct.Object* %1) #0 {
  %3 = alloca i8, align 1
  %4 = alloca %struct.Object*, align 8
  %5 = alloca %struct.Object*, align 8
  %6 = alloca i8*, align 8
  %7 = alloca i8*, align 8
  store %struct.Object* %0, %struct.Object** %4, align 8
  store %struct.Object* %1, %struct.Object** %5, align 8
  %8 = load %struct.Object*, %struct.Object** %4, align 8
  %9 = call i8* @data(%struct.Object* %8)
  store i8* %9, i8** %6, align 8
  %10 = load %struct.Object*, %struct.Object** %5, align 8
  %11 = call i8* @data(%struct.Object* %10)
  store i8* %11, i8** %7, align 8
  %12 = load %struct.Object*, %struct.Object** %4, align 8
  %13 = load %struct.Object*, %struct.Object** %5, align 8
  %14 = icmp eq %struct.Object* %12, %13
  br i1 %14, label %19, label %15

15:                                               ; preds = %2
  %16 = load i8*, i8** %6, align 8
  %17 = load i8*, i8** %7, align 8
  %18 = icmp eq i8* %16, %17
  br i1 %18, label %19, label %20

19:                                               ; preds = %15, %2
  store i8 1, i8* %3, align 1
  br label %85

20:                                               ; preds = %15
  %21 = load %struct.Object*, %struct.Object** %4, align 8
  %22 = getelementptr inbounds %struct.Object, %struct.Object* %21, i32 0, i32 0
  %23 = load i8, i8* %22, align 8
  %24 = zext i8 %23 to i32
  %25 = load %struct.Object*, %struct.Object** %5, align 8
  %26 = getelementptr inbounds %struct.Object, %struct.Object* %25, i32 0, i32 0
  %27 = load i8, i8* %26, align 8
  %28 = zext i8 %27 to i32
  %29 = icmp ne i32 %24, %28
  br i1 %29, label %30, label %31

30:                                               ; preds = %20
  store i8 0, i8* %3, align 1
  br label %85

31:                                               ; preds = %20
  %32 = load %struct.Object*, %struct.Object** %4, align 8
  %33 = getelementptr inbounds %struct.Object, %struct.Object* %32, i32 0, i32 0
  %34 = load i8, i8* %33, align 8
  %35 = zext i8 %34 to i32
  switch i32 %35, label %85 [
    i32 1, label %36
    i32 2, label %42
    i32 3, label %48
    i32 4, label %54
    i32 5, label %60
    i32 6, label %66
    i32 8, label %72
    i32 7, label %78
    i32 0, label %84
  ]

36:                                               ; preds = %31
  %37 = load i8*, i8** %6, align 8
  %38 = bitcast i8* %37 to %struct.Integer*
  %39 = load i8*, i8** %7, align 8
  %40 = bitcast i8* %39 to %struct.Integer*
  %41 = call zeroext i8 @Integer_equals(%struct.Integer* %38, %struct.Integer* %40)
  store i8 %41, i8* %3, align 1
  br label %85

42:                                               ; preds = %31
  %43 = load i8*, i8** %6, align 8
  %44 = bitcast i8* %43 to %struct.String*
  %45 = load i8*, i8** %7, align 8
  %46 = bitcast i8* %45 to %struct.String*
  %47 = call zeroext i8 @String_equals(%struct.String* %44, %struct.String* %46)
  store i8 %47, i8* %3, align 1
  br label %85

48:                                               ; preds = %31
  %49 = load i8*, i8** %6, align 8
  %50 = bitcast i8* %49 to %struct.PersistentList*
  %51 = load i8*, i8** %7, align 8
  %52 = bitcast i8* %51 to %struct.PersistentList*
  %53 = call zeroext i8 @PersistentList_equals(%struct.PersistentList* %50, %struct.PersistentList* %52)
  store i8 %53, i8* %3, align 1
  br label %85

54:                                               ; preds = %31
  %55 = load i8*, i8** %6, align 8
  %56 = bitcast i8* %55 to %struct.PersistentVector*
  %57 = load i8*, i8** %7, align 8
  %58 = bitcast i8* %57 to %struct.PersistentVector*
  %59 = call zeroext i8 @PersistentVector_equals(%struct.PersistentVector* %56, %struct.PersistentVector* %58)
  store i8 %59, i8* %3, align 1
  br label %85

60:                                               ; preds = %31
  %61 = load i8*, i8** %6, align 8
  %62 = bitcast i8* %61 to %struct.PersistentVectorNode*
  %63 = load i8*, i8** %7, align 8
  %64 = bitcast i8* %63 to %struct.PersistentVectorNode*
  %65 = call zeroext i8 @PersistentVectorNode_equals(%struct.PersistentVectorNode* %62, %struct.PersistentVectorNode* %64)
  store i8 %65, i8* %3, align 1
  br label %85

66:                                               ; preds = %31
  %67 = load i8*, i8** %6, align 8
  %68 = bitcast i8* %67 to %struct.Double*
  %69 = load i8*, i8** %7, align 8
  %70 = bitcast i8* %69 to %struct.Double*
  %71 = call zeroext i8 @Double_equals(%struct.Double* %68, %struct.Double* %70)
  store i8 %71, i8* %3, align 1
  br label %85

72:                                               ; preds = %31
  %73 = load i8*, i8** %6, align 8
  %74 = bitcast i8* %73 to %struct.Boolean*
  %75 = load i8*, i8** %7, align 8
  %76 = bitcast i8* %75 to %struct.Boolean*
  %77 = call zeroext i8 @Boolean_equals(%struct.Boolean* %74, %struct.Boolean* %76)
  store i8 %77, i8* %3, align 1
  br label %85

78:                                               ; preds = %31
  %79 = load i8*, i8** %6, align 8
  %80 = bitcast i8* %79 to %struct.Nil*
  %81 = load i8*, i8** %7, align 8
  %82 = bitcast i8* %81 to %struct.Nil*
  %83 = call zeroext i8 @Nil_equals(%struct.Nil* %80, %struct.Nil* %82)
  store i8 %83, i8* %3, align 1
  br label %85

84:                                               ; preds = %31
  call void @__assert_rtn(i8* getelementptr inbounds ([7 x i8], [7 x i8]* @__func__.equals, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str, i64 0, i64 0), i32 160, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.1, i64 0, i64 0)) #5
  unreachable

85:                                               ; preds = %19, %30, %36, %42, %48, %54, %60, %66, %72, %78, %31
  %86 = load i8, i8* %3, align 1
  ret i8 %86
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i64 @hash(%struct.Object* %0) #0 {
  %2 = alloca i64, align 8
  %3 = alloca %struct.Object*, align 8
  store %struct.Object* %0, %struct.Object** %3, align 8
  %4 = load %struct.Object*, %struct.Object** %3, align 8
  %5 = getelementptr inbounds %struct.Object, %struct.Object* %4, i32 0, i32 0
  %6 = load i8, i8* %5, align 8
  %7 = zext i8 %6 to i32
  switch i32 %7, label %49 [
    i32 1, label %8
    i32 2, label %13
    i32 3, label %18
    i32 4, label %23
    i32 5, label %28
    i32 6, label %33
    i32 8, label %38
    i32 7, label %43
    i32 0, label %48
  ]

8:                                                ; preds = %1
  %9 = load %struct.Object*, %struct.Object** %3, align 8
  %10 = call i8* @data(%struct.Object* %9)
  %11 = bitcast i8* %10 to %struct.Integer*
  %12 = call i64 @Integer_hash(%struct.Integer* %11)
  store i64 %12, i64* %2, align 8
  br label %49

13:                                               ; preds = %1
  %14 = load %struct.Object*, %struct.Object** %3, align 8
  %15 = call i8* @data(%struct.Object* %14)
  %16 = bitcast i8* %15 to %struct.String*
  %17 = call i64 @String_hash(%struct.String* %16)
  store i64 %17, i64* %2, align 8
  br label %49

18:                                               ; preds = %1
  %19 = load %struct.Object*, %struct.Object** %3, align 8
  %20 = call i8* @data(%struct.Object* %19)
  %21 = bitcast i8* %20 to %struct.PersistentList*
  %22 = call i64 @PersistentList_hash(%struct.PersistentList* %21)
  store i64 %22, i64* %2, align 8
  br label %49

23:                                               ; preds = %1
  %24 = load %struct.Object*, %struct.Object** %3, align 8
  %25 = call i8* @data(%struct.Object* %24)
  %26 = bitcast i8* %25 to %struct.PersistentVector*
  %27 = call i64 @PersistentVector_hash(%struct.PersistentVector* %26)
  store i64 %27, i64* %2, align 8
  br label %49

28:                                               ; preds = %1
  %29 = load %struct.Object*, %struct.Object** %3, align 8
  %30 = call i8* @data(%struct.Object* %29)
  %31 = bitcast i8* %30 to %struct.PersistentVector*
  %32 = call i64 @PersistentVector_hash(%struct.PersistentVector* %31)
  store i64 %32, i64* %2, align 8
  br label %49

33:                                               ; preds = %1
  %34 = load %struct.Object*, %struct.Object** %3, align 8
  %35 = call i8* @data(%struct.Object* %34)
  %36 = bitcast i8* %35 to %struct.Double*
  %37 = call i64 @Double_hash(%struct.Double* %36)
  store i64 %37, i64* %2, align 8
  br label %49

38:                                               ; preds = %1
  %39 = load %struct.Object*, %struct.Object** %3, align 8
  %40 = call i8* @data(%struct.Object* %39)
  %41 = bitcast i8* %40 to %struct.Boolean*
  %42 = call i64 @Boolean_hash(%struct.Boolean* %41)
  store i64 %42, i64* %2, align 8
  br label %49

43:                                               ; preds = %1
  %44 = load %struct.Object*, %struct.Object** %3, align 8
  %45 = call i8* @data(%struct.Object* %44)
  %46 = bitcast i8* %45 to %struct.Nil*
  %47 = call i64 @Nil_hash(%struct.Nil* %46)
  store i64 %47, i64* %2, align 8
  br label %49

48:                                               ; preds = %1
  call void @__assert_rtn(i8* getelementptr inbounds ([5 x i8], [5 x i8]* @__func__.hash, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str, i64 0, i64 0), i32 192, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.1, i64 0, i64 0)) #5
  unreachable

49:                                               ; preds = %8, %13, %18, %23, %28, %33, %38, %43, %1
  %50 = load i64, i64* %2, align 8
  ret i64 %50
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define %struct.String* @toString(%struct.Object* %0) #0 {
  %2 = alloca %struct.String*, align 8
  %3 = alloca %struct.Object*, align 8
  store %struct.Object* %0, %struct.Object** %3, align 8
  %4 = load %struct.Object*, %struct.Object** %3, align 8
  %5 = getelementptr inbounds %struct.Object, %struct.Object* %4, i32 0, i32 0
  %6 = load i8, i8* %5, align 8
  %7 = zext i8 %6 to i32
  switch i32 %7, label %49 [
    i32 1, label %8
    i32 2, label %13
    i32 3, label %18
    i32 4, label %23
    i32 5, label %28
    i32 6, label %33
    i32 8, label %38
    i32 7, label %43
    i32 0, label %48
  ]

8:                                                ; preds = %1
  %9 = load %struct.Object*, %struct.Object** %3, align 8
  %10 = call i8* @data(%struct.Object* %9)
  %11 = bitcast i8* %10 to %struct.Integer*
  %12 = call %struct.String* @Integer_toString(%struct.Integer* %11)
  store %struct.String* %12, %struct.String** %2, align 8
  br label %49

13:                                               ; preds = %1
  %14 = load %struct.Object*, %struct.Object** %3, align 8
  %15 = call i8* @data(%struct.Object* %14)
  %16 = bitcast i8* %15 to %struct.String*
  %17 = call %struct.String* @String_toString(%struct.String* %16)
  store %struct.String* %17, %struct.String** %2, align 8
  br label %49

18:                                               ; preds = %1
  %19 = load %struct.Object*, %struct.Object** %3, align 8
  %20 = call i8* @data(%struct.Object* %19)
  %21 = bitcast i8* %20 to %struct.PersistentList*
  %22 = call %struct.String* @PersistentList_toString(%struct.PersistentList* %21)
  store %struct.String* %22, %struct.String** %2, align 8
  br label %49

23:                                               ; preds = %1
  %24 = load %struct.Object*, %struct.Object** %3, align 8
  %25 = call i8* @data(%struct.Object* %24)
  %26 = bitcast i8* %25 to %struct.PersistentVector*
  %27 = call %struct.String* @PersistentVector_toString(%struct.PersistentVector* %26)
  store %struct.String* %27, %struct.String** %2, align 8
  br label %49

28:                                               ; preds = %1
  %29 = load %struct.Object*, %struct.Object** %3, align 8
  %30 = call i8* @data(%struct.Object* %29)
  %31 = bitcast i8* %30 to %struct.PersistentVectorNode*
  %32 = call %struct.String* @PersistentVectorNode_toString(%struct.PersistentVectorNode* %31)
  store %struct.String* %32, %struct.String** %2, align 8
  br label %49

33:                                               ; preds = %1
  %34 = load %struct.Object*, %struct.Object** %3, align 8
  %35 = call i8* @data(%struct.Object* %34)
  %36 = bitcast i8* %35 to %struct.Double*
  %37 = call %struct.String* @Double_toString(%struct.Double* %36)
  store %struct.String* %37, %struct.String** %2, align 8
  br label %49

38:                                               ; preds = %1
  %39 = load %struct.Object*, %struct.Object** %3, align 8
  %40 = call i8* @data(%struct.Object* %39)
  %41 = bitcast i8* %40 to %struct.Boolean*
  %42 = call %struct.String* @Boolean_toString(%struct.Boolean* %41)
  store %struct.String* %42, %struct.String** %2, align 8
  br label %49

43:                                               ; preds = %1
  %44 = load %struct.Object*, %struct.Object** %3, align 8
  %45 = call i8* @data(%struct.Object* %44)
  %46 = bitcast i8* %45 to %struct.Nil*
  %47 = call %struct.String* @Nil_toString(%struct.Nil* %46)
  store %struct.String* %47, %struct.String** %2, align 8
  br label %49

48:                                               ; preds = %1
  call void @__assert_rtn(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @__func__.toString, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str, i64 0, i64 0), i32 224, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.1, i64 0, i64 0)) #5
  unreachable

49:                                               ; preds = %8, %13, %18, %23, %28, %33, %38, %43, %1
  %50 = load %struct.String*, %struct.String** %2, align 8
  ret %struct.String* %50
}

; Function Attrs: allocsize(0)
declare i8* @malloc(i64) #2

declare void @free(i8*) #1

declare void @Integer_destroy(%struct.Integer*) #1

declare void @String_destroy(%struct.String*) #1

declare void @PersistentList_destroy(%struct.PersistentList*, i8 zeroext) #1

declare void @PersistentVector_destroy(%struct.PersistentVector*, i8 zeroext) #1

declare void @PersistentVectorNode_destroy(%struct.PersistentVectorNode*, i8 zeroext) #1

declare void @Double_destroy(%struct.Double*) #1

declare void @Boolean_destroy(%struct.Boolean*) #1

declare void @Nil_destroy(%struct.Nil*) #1

; Function Attrs: cold noreturn
declare void @__assert_rtn(i8*, i8*, i32, i8*) #3

declare zeroext i8 @Integer_equals(%struct.Integer*, %struct.Integer*) #1

declare zeroext i8 @String_equals(%struct.String*, %struct.String*) #1

declare zeroext i8 @PersistentList_equals(%struct.PersistentList*, %struct.PersistentList*) #1

declare zeroext i8 @PersistentVector_equals(%struct.PersistentVector*, %struct.PersistentVector*) #1

declare zeroext i8 @PersistentVectorNode_equals(%struct.PersistentVectorNode*, %struct.PersistentVectorNode*) #1

declare zeroext i8 @Double_equals(%struct.Double*, %struct.Double*) #1

declare zeroext i8 @Boolean_equals(%struct.Boolean*, %struct.Boolean*) #1

declare zeroext i8 @Nil_equals(%struct.Nil*, %struct.Nil*) #1

declare i64 @Integer_hash(%struct.Integer*) #1

declare i64 @String_hash(%struct.String*) #1

declare i64 @PersistentList_hash(%struct.PersistentList*) #1

declare i64 @PersistentVector_hash(%struct.PersistentVector*) #1

declare i64 @Double_hash(%struct.Double*) #1

declare i64 @Boolean_hash(%struct.Boolean*) #1

declare i64 @Nil_hash(%struct.Nil*) #1

declare %struct.String* @Integer_toString(%struct.Integer*) #1

declare %struct.String* @String_toString(%struct.String*) #1

declare %struct.String* @PersistentList_toString(%struct.PersistentList*) #1

declare %struct.String* @PersistentVector_toString(%struct.PersistentVector*) #1

declare %struct.String* @PersistentVectorNode_toString(%struct.PersistentVectorNode*) #1

declare %struct.String* @Double_toString(%struct.Double*) #1

declare %struct.String* @Boolean_toString(%struct.Boolean*) #1

declare %struct.String* @Nil_toString(%struct.Nil*) #1

attributes #0 = { noinline nounwind optnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #1 = { "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #2 = { allocsize(0) "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #3 = { cold noreturn "darwin-stkchk-strong-link" "disable-tail-calls"="true" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #4 = { allocsize(0) }
attributes #5 = { cold noreturn }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 12, i32 3]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 13.1.6 (clang-1316.0.21.2.5)"}
