; ModuleID = 'Object.c'
source_filename = "Object.c"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx12.0.0"

%struct.Object = type { i32, i64 }
%struct.String = type { i64, i64, i32, [0 x i8] }
%struct.Integer = type { i64 }
%struct.PersistentList = type { i8*, %struct.PersistentList*, i64 }
%struct.PersistentVector = type { i64, i64, %struct.PersistentVectorNode*, %struct.PersistentVectorNode* }
%struct.PersistentVectorNode = type { i32, i64, [0 x %struct.Object*] }
%struct.Double = type { double }
%struct.Boolean = type { i8 }
%struct.Symbol = type { %struct.String* }
%struct.Nil = type {}
%struct.ConcurrentHashMap = type { %struct.ConcurrentHashMapNode* }
%struct.ConcurrentHashMapNode = type { i64, i16, [0 x %struct.ConcurrentHashMapEntry] }
%struct.ConcurrentHashMapEntry = type { %struct.Object*, %struct.Object*, i64, i16 }
%struct.Keyword = type { %struct.String* }
%struct.Function = type { i64, i64, i64, [0 x %struct.FunctionMethod] }
%struct.FunctionMethod = type { i8, i64, i64, i8*, [3 x %struct.InvokationCache] }
%struct.InvokationCache = type { i64, i8, i8, i8* }
%struct.PersistentArrayMap = type { i64, [8 x i8*], [8 x i8*] }

@allocationCount = common global [14 x i64] zeroinitializer, align 16
@__func__.allocate = private unnamed_addr constant [9 x i8] c"allocate\00", align 1
@.str.1 = private unnamed_addr constant [9 x i8] c"Object.h\00", align 1
@.str.2 = private unnamed_addr constant [23 x i8] c"size > 0 && \22Size = 0\22\00", align 1
@.str.4 = private unnamed_addr constant [43 x i8] c"retVal && \22Could not aloocate that much! \22\00", align 1
@__func__.Object_release_internal = private unnamed_addr constant [24 x i8] c"Object_release_internal\00", align 1
@.str.6 = private unnamed_addr constant [36 x i8] c"relVal >= 1 && \22Memory corruption!\22\00", align 1
@.str.7 = private unnamed_addr constant [38 x i8] c"--> Deallocating type %d addres %lld\0A\00", align 1
@.str.8 = private unnamed_addr constant [18 x i8] c"dealloc end %lld\0A\00", align 1
@str = private unnamed_addr constant [26 x i8] c"=========================\00", align 1

; Function Attrs: nounwind ssp uwtable
define void @initialise_memory() local_unnamed_addr #0 {
  %1 = atomicrmw xchg i64* getelementptr inbounds ([14 x i64], [14 x i64]* @allocationCount, i64 0, i64 0), i64 0 seq_cst, align 8
  %2 = atomicrmw xchg i64* getelementptr inbounds ([14 x i64], [14 x i64]* @allocationCount, i64 0, i64 1), i64 0 seq_cst, align 8
  %3 = atomicrmw xchg i64* getelementptr inbounds ([14 x i64], [14 x i64]* @allocationCount, i64 0, i64 2), i64 0 seq_cst, align 8
  %4 = atomicrmw xchg i64* getelementptr inbounds ([14 x i64], [14 x i64]* @allocationCount, i64 0, i64 3), i64 0 seq_cst, align 8
  %5 = atomicrmw xchg i64* getelementptr inbounds ([14 x i64], [14 x i64]* @allocationCount, i64 0, i64 4), i64 0 seq_cst, align 8
  %6 = atomicrmw xchg i64* getelementptr inbounds ([14 x i64], [14 x i64]* @allocationCount, i64 0, i64 5), i64 0 seq_cst, align 8
  %7 = atomicrmw xchg i64* getelementptr inbounds ([14 x i64], [14 x i64]* @allocationCount, i64 0, i64 6), i64 0 seq_cst, align 8
  %8 = atomicrmw xchg i64* getelementptr inbounds ([14 x i64], [14 x i64]* @allocationCount, i64 0, i64 7), i64 0 seq_cst, align 8
  %9 = atomicrmw xchg i64* getelementptr inbounds ([14 x i64], [14 x i64]* @allocationCount, i64 0, i64 8), i64 0 seq_cst, align 8
  %10 = atomicrmw xchg i64* getelementptr inbounds ([14 x i64], [14 x i64]* @allocationCount, i64 0, i64 9), i64 0 seq_cst, align 8
  tail call void (...) @PersistentVector_initialise() #14
  tail call void (...) @Nil_initialise() #14
  tail call void (...) @Interface_initialise() #14
  ret void
}

declare void @PersistentVector_initialise(...) local_unnamed_addr #1

declare void @Nil_initialise(...) local_unnamed_addr #1

declare void @Interface_initialise(...) local_unnamed_addr #1

; Function Attrs: inlinehint nounwind ssp uwtable
define noalias i8* @allocate(i64 %0) local_unnamed_addr #2 {
  %2 = icmp eq i64 %0, 0
  br i1 %2, label %3, label %4, !prof !6

3:                                                ; preds = %1
  call void @allocate.cold.1() #15
  unreachable

4:                                                ; preds = %1
  %5 = tail call i8* @malloc(i64 %0) #16
  %6 = icmp eq i8* %5, null
  br i1 %6, label %7, label %8, !prof !6

7:                                                ; preds = %4
  call void @allocate.cold.2() #15
  unreachable

8:                                                ; preds = %4
  ret i8* %5
}

; Function Attrs: inlinehint mustprogress nounwind ssp uwtable willreturn
define void @deallocate(i8* noalias nocapture %0) local_unnamed_addr #3 {
  tail call void @free(i8* %0)
  ret void
}

; Function Attrs: inlinehint mustprogress nofree norecurse nosync nounwind readnone ssp uwtable willreturn
define %struct.Object* @super(i8* noalias readnone %0) local_unnamed_addr #4 {
  %2 = getelementptr i8, i8* %0, i64 -16
  %3 = bitcast i8* %2 to %struct.Object*
  ret %struct.Object* %3
}

; Function Attrs: inlinehint mustprogress nofree nounwind ssp uwtable willreturn
define void @retain(i8* noalias %0) local_unnamed_addr #5 {
  %2 = getelementptr i8, i8* %0, i64 -8
  %3 = bitcast i8* %2 to i64*
  %4 = atomicrmw volatile add i64* %3, i64 1 monotonic, align 8, !alias.scope !7
  ret void
}

; Function Attrs: inlinehint nounwind ssp uwtable
define zeroext i8 @release(i8* noalias %0) local_unnamed_addr #2 {
  %2 = getelementptr i8, i8* %0, i64 -8
  %3 = bitcast i8* %2 to i64*
  %4 = atomicrmw volatile sub i64* %3, i64 1 monotonic, align 8, !alias.scope !10
  switch i64 %4, label %9 [
    i64 0, label %5
    i64 1, label %6
  ], !prof !15

5:                                                ; preds = %1
  call void @release.cold.1() #15
  unreachable

6:                                                ; preds = %1
  %7 = getelementptr i8, i8* %0, i64 -16
  %8 = bitcast i8* %7 to %struct.Object*
  tail call void @Object_destroy(%struct.Object* %8, i8 zeroext 1) #14
  br label %9

9:                                                ; preds = %1, %6
  %10 = phi i8 [ 1, %6 ], [ 0, %1 ]
  ret i8 %10
}

; Function Attrs: inlinehint nounwind ssp uwtable
define void @autorelease(i8* noalias %0) local_unnamed_addr #2 {
  %2 = getelementptr i8, i8* %0, i64 -16
  %3 = bitcast i8* %2 to %struct.Object*
  %4 = getelementptr i8, i8* %0, i64 -8
  %5 = bitcast i8* %4 to i64*
  %6 = load atomic volatile i64, i64* %5 seq_cst, align 8, !alias.scope !16
  %7 = icmp eq i64 %6, 0
  br i1 %7, label %12, label %8

8:                                                ; preds = %1
  %9 = atomicrmw volatile sub i64* %5, i64 1 monotonic, align 8, !alias.scope !19
  switch i64 %9, label %12 [
    i64 0, label %10
    i64 1, label %11
  ], !prof !15

10:                                               ; preds = %8
  call void @autorelease.cold.1() #15
  unreachable

11:                                               ; preds = %8
  tail call void @Object_destroy(%struct.Object* nonnull %3, i8 zeroext 1) #14
  br label %12

12:                                               ; preds = %1, %8, %11
  ret void
}

; Function Attrs: inlinehint nounwind ssp uwtable
define zeroext i8 @equals(i8* noalias %0, i8* noalias %1) local_unnamed_addr #2 {
  %3 = getelementptr i8, i8* %0, i64 -16
  %4 = bitcast i8* %3 to %struct.Object*
  %5 = getelementptr i8, i8* %1, i64 -16
  %6 = bitcast i8* %5 to %struct.Object*
  %7 = tail call zeroext i8 @Object_equals(%struct.Object* %4, %struct.Object* %6)
  ret i8 %7
}

; Function Attrs: inlinehint nounwind ssp uwtable
define i64 @hash(i8* noalias %0) local_unnamed_addr #2 {
  %2 = getelementptr i8, i8* %0, i64 -16
  %3 = bitcast i8* %2 to %struct.Object*
  %4 = tail call i64 @Object_hash(%struct.Object* %3)
  ret i64 %4
}

; Function Attrs: inlinehint nounwind ssp uwtable
define %struct.String* @toString(i8* noalias %0) local_unnamed_addr #2 {
  %2 = getelementptr i8, i8* %0, i64 -16
  %3 = bitcast i8* %2 to %struct.Object*
  %4 = tail call %struct.String* @Object_toString(%struct.Object* %3)
  ret %struct.String* %4
}

; Function Attrs: inlinehint mustprogress nofree norecurse nosync nounwind readnone ssp uwtable willreturn
define nonnull i8* @Object_data(%struct.Object* noalias readnone %0) local_unnamed_addr #4 {
  %2 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %3 = bitcast %struct.Object* %2 to i8*
  ret i8* %3
}

; Function Attrs: inlinehint nofree norecurse nounwind ssp uwtable
define void @Object_create(%struct.Object* noalias %0, i32 %1) local_unnamed_addr #6 {
  %3 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 1
  store atomic volatile i64 1, i64* %3 monotonic, align 8
  %4 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 0
  store i32 %1, i32* %4, align 8, !tbaa !24
  ret void
}

; Function Attrs: inlinehint mustprogress nofree norecurse nounwind ssp uwtable willreturn
define void @Object_retain(%struct.Object* noalias %0) local_unnamed_addr #7 {
  %2 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 1
  %3 = atomicrmw volatile add i64* %2, i64 1 monotonic, align 8
  ret void
}

; Function Attrs: inlinehint nounwind ssp uwtable
define zeroext i8 @Object_release(%struct.Object* noalias %0) local_unnamed_addr #2 {
  %2 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 1
  %3 = atomicrmw volatile sub i64* %2, i64 1 monotonic, align 8, !alias.scope !28
  switch i64 %3, label %6 [
    i64 0, label %4
    i64 1, label %5
  ], !prof !15

4:                                                ; preds = %1
  call void @Object_release.cold.1() #15
  unreachable

5:                                                ; preds = %1
  tail call void @Object_destroy(%struct.Object* %0, i8 zeroext 1) #14
  br label %6

6:                                                ; preds = %1, %5
  %7 = phi i8 [ 1, %5 ], [ 0, %1 ]
  ret i8 %7
}

; Function Attrs: inlinehint nounwind ssp uwtable
define zeroext i8 @Object_release_internal(%struct.Object* noalias %0, i8 zeroext %1) local_unnamed_addr #2 {
  %3 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 1
  %4 = atomicrmw volatile sub i64* %3, i64 1 monotonic, align 8
  switch i64 %4, label %7 [
    i64 0, label %5
    i64 1, label %6
  ], !prof !15

5:                                                ; preds = %2
  call void @Object_release_internal.cold.1() #15
  unreachable

6:                                                ; preds = %2
  tail call void @Object_destroy(%struct.Object* %0, i8 zeroext %1)
  br label %7

7:                                                ; preds = %2, %6
  %8 = phi i8 [ 1, %6 ], [ 0, %2 ]
  ret i8 %8
}

; Function Attrs: inlinehint nounwind ssp uwtable
define void @Object_destroy(%struct.Object* noalias %0, i8 zeroext %1) local_unnamed_addr #2 {
  %3 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 0
  %4 = load i32, i32* %3, align 8, !tbaa !24
  %5 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %6 = ptrtoint %struct.Object* %5 to i64
  %7 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([38 x i8], [38 x i8]* @.str.7, i64 0, i64 0), i32 %4, i64 %6)
  tail call void (...) @printReferenceCounts() #14
  %8 = load i32, i32* %3, align 8, !tbaa !24
  switch i32 %8, label %35 [
    i32 1, label %9
    i32 2, label %11
    i32 3, label %13
    i32 4, label %15
    i32 5, label %17
    i32 6, label %19
    i32 8, label %21
    i32 9, label %23
    i32 7, label %25
    i32 10, label %27
    i32 11, label %29
    i32 12, label %31
    i32 13, label %33
  ]

9:                                                ; preds = %2
  %10 = bitcast %struct.Object* %5 to %struct.Integer*
  tail call void @Integer_destroy(%struct.Integer* nonnull %10) #14
  br label %35

11:                                               ; preds = %2
  %12 = bitcast %struct.Object* %5 to %struct.String*
  tail call void @String_destroy(%struct.String* nonnull %12) #14
  br label %35

13:                                               ; preds = %2
  %14 = bitcast %struct.Object* %5 to %struct.PersistentList*
  tail call void @PersistentList_destroy(%struct.PersistentList* nonnull %14, i8 zeroext %1) #14
  br label %35

15:                                               ; preds = %2
  %16 = bitcast %struct.Object* %5 to %struct.PersistentVector*
  tail call void @PersistentVector_destroy(%struct.PersistentVector* nonnull %16, i8 zeroext %1) #14
  br label %35

17:                                               ; preds = %2
  %18 = bitcast %struct.Object* %5 to %struct.PersistentVectorNode*
  tail call void @PersistentVectorNode_destroy(%struct.PersistentVectorNode* nonnull %18, i8 zeroext %1) #14
  br label %35

19:                                               ; preds = %2
  %20 = bitcast %struct.Object* %5 to %struct.Double*
  tail call void @Double_destroy(%struct.Double* nonnull %20) #14
  br label %35

21:                                               ; preds = %2
  %22 = bitcast %struct.Object* %5 to %struct.Boolean*
  tail call void @Boolean_destroy(%struct.Boolean* nonnull %22) #14
  br label %35

23:                                               ; preds = %2
  %24 = bitcast %struct.Object* %5 to %struct.Symbol*
  tail call void @Symbol_destroy(%struct.Symbol* nonnull %24) #14
  br label %35

25:                                               ; preds = %2
  %26 = bitcast %struct.Object* %5 to %struct.Nil*
  tail call void @Nil_destroy(%struct.Nil* nonnull %26) #14
  br label %35

27:                                               ; preds = %2
  %28 = bitcast %struct.Object* %5 to %struct.ConcurrentHashMap*
  tail call void @ConcurrentHashMap_destroy(%struct.ConcurrentHashMap* nonnull %28) #14
  br label %35

29:                                               ; preds = %2
  %30 = bitcast %struct.Object* %5 to %struct.Keyword*
  tail call void @Keyword_destroy(%struct.Keyword* nonnull %30) #14
  br label %35

31:                                               ; preds = %2
  %32 = bitcast %struct.Object* %5 to %struct.Function*
  tail call void @Function_destroy(%struct.Function* nonnull %32) #14
  br label %35

33:                                               ; preds = %2
  %34 = bitcast %struct.Object* %5 to %struct.PersistentArrayMap*
  tail call void @PersistentArrayMap_destroy(%struct.PersistentArrayMap* nonnull %34, i8 zeroext %1) #14
  br label %35

35:                                               ; preds = %2, %33, %31, %29, %27, %25, %23, %21, %19, %17, %15, %13, %11, %9
  %36 = bitcast %struct.Object* %0 to i8*
  tail call void @free(i8* %36) #14
  %37 = tail call i32 (i8*, ...) @printf(i8* nonnull dereferenceable(1) getelementptr inbounds ([18 x i8], [18 x i8]* @.str.8, i64 0, i64 0), i64 %6)
  tail call void (...) @printReferenceCounts() #14
  %38 = tail call i32 @puts(i8* nonnull dereferenceable(1) getelementptr inbounds ([26 x i8], [26 x i8]* @str, i64 0, i64 0))
  ret void
}

; Function Attrs: inlinehint nounwind ssp uwtable
define void @Object_autorelease(%struct.Object* noalias %0) local_unnamed_addr #2 {
  %2 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 1
  %3 = load atomic volatile i64, i64* %2 seq_cst, align 8
  %4 = icmp eq i64 %3, 0
  br i1 %4, label %9, label %5

5:                                                ; preds = %1
  %6 = atomicrmw volatile sub i64* %2, i64 1 monotonic, align 8, !alias.scope !31
  switch i64 %6, label %9 [
    i64 0, label %7
    i64 1, label %8
  ], !prof !15

7:                                                ; preds = %5
  call void @Object_autorelease.cold.1() #15
  unreachable

8:                                                ; preds = %5
  tail call void @Object_destroy(%struct.Object* nonnull %0, i8 zeroext 1) #14
  br label %9

9:                                                ; preds = %8, %5, %1
  ret void
}

; Function Attrs: inlinehint nounwind ssp uwtable
define zeroext i8 @Object_equals(%struct.Object* noalias %0, %struct.Object* noalias %1) local_unnamed_addr #2 {
  %3 = icmp eq %struct.Object* %0, %1
  br i1 %3, label %70, label %4

4:                                                ; preds = %2
  %5 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 0
  %6 = load i32, i32* %5, align 8, !tbaa !24
  %7 = getelementptr inbounds %struct.Object, %struct.Object* %1, i64 0, i32 0
  %8 = load i32, i32* %7, align 8, !tbaa !24
  %9 = icmp eq i32 %6, %8
  br i1 %9, label %10, label %70

10:                                               ; preds = %4
  %11 = tail call i64 @Object_hash(%struct.Object* nonnull %0)
  %12 = tail call i64 @Object_hash(%struct.Object* nonnull %1)
  %13 = icmp eq i64 %11, %12
  br i1 %13, label %14, label %70

14:                                               ; preds = %10
  %15 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %16 = getelementptr inbounds %struct.Object, %struct.Object* %1, i64 1
  %17 = load i32, i32* %5, align 8, !tbaa !24
  switch i32 %17, label %70 [
    i32 1, label %18
    i32 2, label %22
    i32 3, label %26
    i32 4, label %30
    i32 5, label %34
    i32 6, label %38
    i32 8, label %42
    i32 7, label %46
    i32 9, label %50
    i32 10, label %54
    i32 11, label %58
    i32 12, label %62
    i32 13, label %66
  ]

18:                                               ; preds = %14
  %19 = bitcast %struct.Object* %15 to %struct.Integer*
  %20 = bitcast %struct.Object* %16 to %struct.Integer*
  %21 = tail call zeroext i8 @Integer_equals(%struct.Integer* nonnull %19, %struct.Integer* nonnull %20) #14
  br label %70

22:                                               ; preds = %14
  %23 = bitcast %struct.Object* %15 to %struct.String*
  %24 = bitcast %struct.Object* %16 to %struct.String*
  %25 = tail call zeroext i8 @String_equals(%struct.String* nonnull %23, %struct.String* nonnull %24) #14
  br label %70

26:                                               ; preds = %14
  %27 = bitcast %struct.Object* %15 to %struct.PersistentList*
  %28 = bitcast %struct.Object* %16 to %struct.PersistentList*
  %29 = tail call zeroext i8 @PersistentList_equals(%struct.PersistentList* nonnull %27, %struct.PersistentList* nonnull %28) #14
  br label %70

30:                                               ; preds = %14
  %31 = bitcast %struct.Object* %15 to %struct.PersistentVector*
  %32 = bitcast %struct.Object* %16 to %struct.PersistentVector*
  %33 = tail call zeroext i8 @PersistentVector_equals(%struct.PersistentVector* nonnull %31, %struct.PersistentVector* nonnull %32) #14
  br label %70

34:                                               ; preds = %14
  %35 = bitcast %struct.Object* %15 to %struct.PersistentVectorNode*
  %36 = bitcast %struct.Object* %16 to %struct.PersistentVectorNode*
  %37 = tail call zeroext i8 @PersistentVectorNode_equals(%struct.PersistentVectorNode* nonnull %35, %struct.PersistentVectorNode* nonnull %36) #14
  br label %70

38:                                               ; preds = %14
  %39 = bitcast %struct.Object* %15 to %struct.Double*
  %40 = bitcast %struct.Object* %16 to %struct.Double*
  %41 = tail call zeroext i8 @Double_equals(%struct.Double* nonnull %39, %struct.Double* nonnull %40) #14
  br label %70

42:                                               ; preds = %14
  %43 = bitcast %struct.Object* %15 to %struct.Boolean*
  %44 = bitcast %struct.Object* %16 to %struct.Boolean*
  %45 = tail call zeroext i8 @Boolean_equals(%struct.Boolean* nonnull %43, %struct.Boolean* nonnull %44) #14
  br label %70

46:                                               ; preds = %14
  %47 = bitcast %struct.Object* %15 to %struct.Nil*
  %48 = bitcast %struct.Object* %16 to %struct.Nil*
  %49 = tail call zeroext i8 @Nil_equals(%struct.Nil* nonnull %47, %struct.Nil* nonnull %48) #14
  br label %70

50:                                               ; preds = %14
  %51 = bitcast %struct.Object* %15 to %struct.Symbol*
  %52 = bitcast %struct.Object* %16 to %struct.Symbol*
  %53 = tail call zeroext i8 @Symbol_equals(%struct.Symbol* nonnull %51, %struct.Symbol* nonnull %52) #14
  br label %70

54:                                               ; preds = %14
  %55 = bitcast %struct.Object* %15 to %struct.ConcurrentHashMap*
  %56 = bitcast %struct.Object* %16 to %struct.ConcurrentHashMap*
  %57 = tail call zeroext i8 @ConcurrentHashMap_equals(%struct.ConcurrentHashMap* nonnull %55, %struct.ConcurrentHashMap* nonnull %56) #14
  br label %70

58:                                               ; preds = %14
  %59 = bitcast %struct.Object* %15 to %struct.Keyword*
  %60 = bitcast %struct.Object* %16 to %struct.Keyword*
  %61 = tail call zeroext i8 @Keyword_equals(%struct.Keyword* nonnull %59, %struct.Keyword* nonnull %60) #14
  br label %70

62:                                               ; preds = %14
  %63 = bitcast %struct.Object* %15 to %struct.Function*
  %64 = bitcast %struct.Object* %16 to %struct.Function*
  %65 = tail call zeroext i8 @Function_equals(%struct.Function* nonnull %63, %struct.Function* nonnull %64) #14
  br label %70

66:                                               ; preds = %14
  %67 = bitcast %struct.Object* %15 to %struct.PersistentArrayMap*
  %68 = bitcast %struct.Object* %16 to %struct.PersistentArrayMap*
  %69 = tail call zeroext i8 @PersistentArrayMap_equals(%struct.PersistentArrayMap* nonnull %67, %struct.PersistentArrayMap* nonnull %68) #14
  br label %70

70:                                               ; preds = %18, %22, %26, %30, %34, %38, %42, %46, %50, %54, %58, %62, %66, %14, %10, %4, %2
  %71 = phi i8 [ 1, %2 ], [ 0, %4 ], [ 0, %10 ], [ %69, %66 ], [ %65, %62 ], [ %61, %58 ], [ %57, %54 ], [ %53, %50 ], [ %49, %46 ], [ %45, %42 ], [ %41, %38 ], [ %37, %34 ], [ %33, %30 ], [ %29, %26 ], [ %25, %22 ], [ %21, %18 ], [ undef, %14 ]
  ret i8 %71
}

; Function Attrs: inlinehint nounwind ssp uwtable
define i64 @Object_hash(%struct.Object* noalias %0) local_unnamed_addr #2 {
  %2 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 0
  %3 = load i32, i32* %2, align 8, !tbaa !24
  switch i32 %3, label %56 [
    i32 1, label %4
    i32 2, label %8
    i32 3, label %12
    i32 4, label %16
    i32 5, label %20
    i32 6, label %24
    i32 8, label %28
    i32 7, label %32
    i32 9, label %36
    i32 10, label %40
    i32 11, label %44
    i32 12, label %48
    i32 13, label %52
  ]

4:                                                ; preds = %1
  %5 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %6 = bitcast %struct.Object* %5 to %struct.Integer*
  %7 = tail call i64 @Integer_hash(%struct.Integer* nonnull %6) #14
  br label %56

8:                                                ; preds = %1
  %9 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %10 = bitcast %struct.Object* %9 to %struct.String*
  %11 = tail call i64 @String_hash(%struct.String* nonnull %10) #14
  br label %56

12:                                               ; preds = %1
  %13 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %14 = bitcast %struct.Object* %13 to %struct.PersistentList*
  %15 = tail call i64 @PersistentList_hash(%struct.PersistentList* nonnull %14) #14
  br label %56

16:                                               ; preds = %1
  %17 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %18 = bitcast %struct.Object* %17 to %struct.PersistentVector*
  %19 = tail call i64 @PersistentVector_hash(%struct.PersistentVector* nonnull %18) #14
  br label %56

20:                                               ; preds = %1
  %21 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %22 = bitcast %struct.Object* %21 to %struct.PersistentVector*
  %23 = tail call i64 @PersistentVector_hash(%struct.PersistentVector* nonnull %22) #14
  br label %56

24:                                               ; preds = %1
  %25 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %26 = bitcast %struct.Object* %25 to %struct.Double*
  %27 = tail call i64 @Double_hash(%struct.Double* nonnull %26) #14
  br label %56

28:                                               ; preds = %1
  %29 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %30 = bitcast %struct.Object* %29 to %struct.Boolean*
  %31 = tail call i64 @Boolean_hash(%struct.Boolean* nonnull %30) #14
  br label %56

32:                                               ; preds = %1
  %33 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %34 = bitcast %struct.Object* %33 to %struct.Nil*
  %35 = tail call i64 @Nil_hash(%struct.Nil* nonnull %34) #14
  br label %56

36:                                               ; preds = %1
  %37 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %38 = bitcast %struct.Object* %37 to %struct.Symbol*
  %39 = tail call i64 @Symbol_hash(%struct.Symbol* nonnull %38) #14
  br label %56

40:                                               ; preds = %1
  %41 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %42 = bitcast %struct.Object* %41 to %struct.ConcurrentHashMap*
  %43 = tail call i64 @ConcurrentHashMap_hash(%struct.ConcurrentHashMap* nonnull %42) #14
  br label %56

44:                                               ; preds = %1
  %45 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %46 = bitcast %struct.Object* %45 to %struct.Keyword*
  %47 = tail call i64 @Keyword_hash(%struct.Keyword* nonnull %46) #14
  br label %56

48:                                               ; preds = %1
  %49 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %50 = bitcast %struct.Object* %49 to %struct.Function*
  %51 = tail call i64 @Function_hash(%struct.Function* nonnull %50) #14
  br label %56

52:                                               ; preds = %1
  %53 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %54 = bitcast %struct.Object* %53 to %struct.PersistentArrayMap*
  %55 = tail call i64 @PersistentArrayMap_hash(%struct.PersistentArrayMap* nonnull %54) #14
  br label %56

56:                                               ; preds = %4, %8, %12, %16, %20, %24, %28, %32, %36, %40, %44, %48, %52, %1
  %57 = phi i64 [ undef, %1 ], [ %55, %52 ], [ %51, %48 ], [ %47, %44 ], [ %43, %40 ], [ %39, %36 ], [ %35, %32 ], [ %31, %28 ], [ %27, %24 ], [ %23, %20 ], [ %19, %16 ], [ %15, %12 ], [ %11, %8 ], [ %7, %4 ]
  ret i64 %57
}

; Function Attrs: inlinehint nounwind ssp uwtable
define %struct.String* @Object_toString(%struct.Object* noalias %0) local_unnamed_addr #2 {
  %2 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 0
  %3 = load i32, i32* %2, align 8, !tbaa !24
  switch i32 %3, label %56 [
    i32 1, label %4
    i32 2, label %8
    i32 3, label %12
    i32 4, label %16
    i32 5, label %20
    i32 6, label %24
    i32 8, label %28
    i32 7, label %32
    i32 9, label %36
    i32 10, label %40
    i32 11, label %44
    i32 12, label %48
    i32 13, label %52
  ]

4:                                                ; preds = %1
  %5 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %6 = bitcast %struct.Object* %5 to %struct.Integer*
  %7 = tail call %struct.String* @Integer_toString(%struct.Integer* nonnull %6) #14
  br label %56

8:                                                ; preds = %1
  %9 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %10 = bitcast %struct.Object* %9 to %struct.String*
  %11 = tail call %struct.String* @String_toString(%struct.String* nonnull %10) #14
  br label %56

12:                                               ; preds = %1
  %13 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %14 = bitcast %struct.Object* %13 to %struct.PersistentList*
  %15 = tail call %struct.String* @PersistentList_toString(%struct.PersistentList* nonnull %14) #14
  br label %56

16:                                               ; preds = %1
  %17 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %18 = bitcast %struct.Object* %17 to %struct.PersistentVector*
  %19 = tail call %struct.String* @PersistentVector_toString(%struct.PersistentVector* nonnull %18) #14
  br label %56

20:                                               ; preds = %1
  %21 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %22 = bitcast %struct.Object* %21 to %struct.PersistentVectorNode*
  %23 = tail call %struct.String* @PersistentVectorNode_toString(%struct.PersistentVectorNode* nonnull %22) #14
  br label %56

24:                                               ; preds = %1
  %25 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %26 = bitcast %struct.Object* %25 to %struct.Double*
  %27 = tail call %struct.String* @Double_toString(%struct.Double* nonnull %26) #14
  br label %56

28:                                               ; preds = %1
  %29 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %30 = bitcast %struct.Object* %29 to %struct.Boolean*
  %31 = tail call %struct.String* @Boolean_toString(%struct.Boolean* nonnull %30) #14
  br label %56

32:                                               ; preds = %1
  %33 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %34 = bitcast %struct.Object* %33 to %struct.Nil*
  %35 = tail call %struct.String* @Nil_toString(%struct.Nil* nonnull %34) #14
  br label %56

36:                                               ; preds = %1
  %37 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %38 = bitcast %struct.Object* %37 to %struct.Symbol*
  %39 = tail call %struct.String* @Symbol_toString(%struct.Symbol* nonnull %38) #14
  br label %56

40:                                               ; preds = %1
  %41 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %42 = bitcast %struct.Object* %41 to %struct.ConcurrentHashMap*
  %43 = tail call %struct.String* @ConcurrentHashMap_toString(%struct.ConcurrentHashMap* nonnull %42) #14
  br label %56

44:                                               ; preds = %1
  %45 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %46 = bitcast %struct.Object* %45 to %struct.Keyword*
  %47 = tail call %struct.String* @Keyword_toString(%struct.Keyword* nonnull %46) #14
  br label %56

48:                                               ; preds = %1
  %49 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %50 = bitcast %struct.Object* %49 to %struct.Function*
  %51 = tail call %struct.String* @Function_toString(%struct.Function* nonnull %50) #14
  br label %56

52:                                               ; preds = %1
  %53 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %54 = bitcast %struct.Object* %53 to %struct.PersistentArrayMap*
  %55 = tail call %struct.String* @PersistentArrayMap_toString(%struct.PersistentArrayMap* nonnull %54) #14
  br label %56

56:                                               ; preds = %4, %8, %12, %16, %20, %24, %28, %32, %36, %40, %44, %48, %52, %1
  %57 = phi %struct.String* [ undef, %1 ], [ %55, %52 ], [ %51, %48 ], [ %47, %44 ], [ %43, %40 ], [ %39, %36 ], [ %35, %32 ], [ %31, %28 ], [ %27, %24 ], [ %23, %20 ], [ %19, %16 ], [ %15, %12 ], [ %11, %8 ], [ %7, %4 ]
  ret %struct.String* %57
}

; Function Attrs: inlinehint mustprogress nofree norecurse nounwind ssp uwtable willreturn
define zeroext i8 @Object_isReusable(%struct.Object* noalias %0) local_unnamed_addr #7 {
  %2 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 1
  %3 = load atomic volatile i64, i64* %2 monotonic, align 8
  %4 = icmp eq i64 %3, 1
  %5 = zext i1 %4 to i8
  ret i8 %5
}

; Function Attrs: inlinehint mustprogress nofree nounwind ssp uwtable willreturn
define zeroext i8 @isReusable(i8* noalias %0) local_unnamed_addr #5 {
  %2 = getelementptr i8, i8* %0, i64 -8
  %3 = bitcast i8* %2 to i64*
  %4 = load atomic volatile i64, i64* %3 monotonic, align 8, !alias.scope !36
  %5 = icmp eq i64 %4, 1
  %6 = zext i1 %5 to i8
  ret i8 %6
}

; Function Attrs: inlinehint mustprogress nofree norecurse nosync nounwind readnone ssp uwtable willreturn
define i64 @combineHash(i64 %0, i64 %1) local_unnamed_addr #4 {
  %3 = add i64 %1, -7070675565921424023
  %4 = shl i64 %0, 6
  %5 = add i64 %3, %4
  %6 = lshr i64 %0, 2
  %7 = add i64 %5, %6
  %8 = xor i64 %7, %0
  ret i64 %8
}

; Function Attrs: cold noreturn
declare void @__assert_rtn(i8*, i8*, i32, i8*) local_unnamed_addr #8

; Function Attrs: inaccessiblememonly mustprogress nofree nounwind willreturn allocsize(0)
declare noalias noundef i8* @malloc(i64 noundef) local_unnamed_addr #9

; Function Attrs: inaccessiblemem_or_argmemonly mustprogress nounwind willreturn
declare void @free(i8* nocapture noundef) local_unnamed_addr #10

; Function Attrs: nofree nounwind
declare noundef i32 @printf(i8* nocapture noundef readonly, ...) local_unnamed_addr #11

declare void @printReferenceCounts(...) local_unnamed_addr #1

declare void @Integer_destroy(%struct.Integer*) local_unnamed_addr #1

declare void @String_destroy(%struct.String*) local_unnamed_addr #1

declare void @PersistentList_destroy(%struct.PersistentList*, i8 zeroext) local_unnamed_addr #1

declare void @PersistentVector_destroy(%struct.PersistentVector*, i8 zeroext) local_unnamed_addr #1

declare void @PersistentVectorNode_destroy(%struct.PersistentVectorNode*, i8 zeroext) local_unnamed_addr #1

declare void @Double_destroy(%struct.Double*) local_unnamed_addr #1

declare void @Boolean_destroy(%struct.Boolean*) local_unnamed_addr #1

declare void @Symbol_destroy(%struct.Symbol*) local_unnamed_addr #1

declare void @Nil_destroy(%struct.Nil*) local_unnamed_addr #1

declare void @ConcurrentHashMap_destroy(%struct.ConcurrentHashMap*) local_unnamed_addr #1

declare void @Keyword_destroy(%struct.Keyword*) local_unnamed_addr #1

declare void @Function_destroy(%struct.Function*) local_unnamed_addr #1

declare void @PersistentArrayMap_destroy(%struct.PersistentArrayMap*, i8 zeroext) local_unnamed_addr #1

declare zeroext i8 @Integer_equals(%struct.Integer*, %struct.Integer*) local_unnamed_addr #1

declare zeroext i8 @String_equals(%struct.String*, %struct.String*) local_unnamed_addr #1

declare zeroext i8 @PersistentList_equals(%struct.PersistentList*, %struct.PersistentList*) local_unnamed_addr #1

declare zeroext i8 @PersistentVector_equals(%struct.PersistentVector*, %struct.PersistentVector*) local_unnamed_addr #1

declare zeroext i8 @PersistentVectorNode_equals(%struct.PersistentVectorNode*, %struct.PersistentVectorNode*) local_unnamed_addr #1

declare zeroext i8 @Double_equals(%struct.Double*, %struct.Double*) local_unnamed_addr #1

declare zeroext i8 @Boolean_equals(%struct.Boolean*, %struct.Boolean*) local_unnamed_addr #1

declare zeroext i8 @Nil_equals(%struct.Nil*, %struct.Nil*) local_unnamed_addr #1

declare zeroext i8 @Symbol_equals(%struct.Symbol*, %struct.Symbol*) local_unnamed_addr #1

declare zeroext i8 @ConcurrentHashMap_equals(%struct.ConcurrentHashMap*, %struct.ConcurrentHashMap*) local_unnamed_addr #1

declare zeroext i8 @Keyword_equals(%struct.Keyword*, %struct.Keyword*) local_unnamed_addr #1

declare zeroext i8 @Function_equals(%struct.Function*, %struct.Function*) local_unnamed_addr #1

declare zeroext i8 @PersistentArrayMap_equals(%struct.PersistentArrayMap*, %struct.PersistentArrayMap*) local_unnamed_addr #1

declare i64 @Integer_hash(%struct.Integer*) local_unnamed_addr #1

declare i64 @String_hash(%struct.String*) local_unnamed_addr #1

declare i64 @PersistentList_hash(%struct.PersistentList*) local_unnamed_addr #1

declare i64 @PersistentVector_hash(%struct.PersistentVector*) local_unnamed_addr #1

declare i64 @Double_hash(%struct.Double*) local_unnamed_addr #1

declare i64 @Boolean_hash(%struct.Boolean*) local_unnamed_addr #1

declare i64 @Nil_hash(%struct.Nil*) local_unnamed_addr #1

declare i64 @Symbol_hash(%struct.Symbol*) local_unnamed_addr #1

declare i64 @ConcurrentHashMap_hash(%struct.ConcurrentHashMap*) local_unnamed_addr #1

declare i64 @Keyword_hash(%struct.Keyword*) local_unnamed_addr #1

declare i64 @Function_hash(%struct.Function*) local_unnamed_addr #1

declare i64 @PersistentArrayMap_hash(%struct.PersistentArrayMap*) local_unnamed_addr #1

declare %struct.String* @Integer_toString(%struct.Integer*) local_unnamed_addr #1

declare %struct.String* @String_toString(%struct.String*) local_unnamed_addr #1

declare %struct.String* @PersistentList_toString(%struct.PersistentList*) local_unnamed_addr #1

declare %struct.String* @PersistentVector_toString(%struct.PersistentVector*) local_unnamed_addr #1

declare %struct.String* @PersistentVectorNode_toString(%struct.PersistentVectorNode*) local_unnamed_addr #1

declare %struct.String* @Double_toString(%struct.Double*) local_unnamed_addr #1

declare %struct.String* @Boolean_toString(%struct.Boolean*) local_unnamed_addr #1

declare %struct.String* @Nil_toString(%struct.Nil*) local_unnamed_addr #1

declare %struct.String* @Symbol_toString(%struct.Symbol*) local_unnamed_addr #1

declare %struct.String* @ConcurrentHashMap_toString(%struct.ConcurrentHashMap*) local_unnamed_addr #1

declare %struct.String* @Keyword_toString(%struct.Keyword*) local_unnamed_addr #1

declare %struct.String* @Function_toString(%struct.Function*) local_unnamed_addr #1

declare %struct.String* @PersistentArrayMap_toString(%struct.PersistentArrayMap*) local_unnamed_addr #1

; Function Attrs: nofree nounwind
declare noundef i32 @puts(i8* nocapture noundef readonly) local_unnamed_addr #12

; Function Attrs: cold inlinehint minsize noreturn nounwind ssp uwtable
define internal void @allocate.cold.1() #13 {
  tail call void @__assert_rtn(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @__func__.allocate, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.1, i64 0, i64 0), i32 55, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0)) #17
  unreachable
}

; Function Attrs: cold inlinehint minsize noreturn nounwind ssp uwtable
define internal void @allocate.cold.2() #13 {
  tail call void @__assert_rtn(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @__func__.allocate, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.1, i64 0, i64 0), i32 57, i8* getelementptr inbounds ([43 x i8], [43 x i8]* @.str.4, i64 0, i64 0)) #17
  unreachable
}

; Function Attrs: cold inlinehint minsize noreturn nounwind ssp uwtable
define internal void @release.cold.1() #13 {
  tail call void @__assert_rtn(i8* getelementptr inbounds ([24 x i8], [24 x i8]* @__func__.Object_release_internal, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.1, i64 0, i64 0), i32 155, i8* getelementptr inbounds ([36 x i8], [36 x i8]* @.str.6, i64 0, i64 0)) #17
  unreachable
}

; Function Attrs: cold inlinehint minsize noreturn nounwind ssp uwtable
define internal void @autorelease.cold.1() #13 {
  tail call void @__assert_rtn(i8* getelementptr inbounds ([24 x i8], [24 x i8]* @__func__.Object_release_internal, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.1, i64 0, i64 0), i32 155, i8* getelementptr inbounds ([36 x i8], [36 x i8]* @.str.6, i64 0, i64 0)) #17
  unreachable
}

; Function Attrs: cold inlinehint minsize noreturn nounwind ssp uwtable
define internal void @Object_release.cold.1() #13 {
  tail call void @__assert_rtn(i8* getelementptr inbounds ([24 x i8], [24 x i8]* @__func__.Object_release_internal, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.1, i64 0, i64 0), i32 155, i8* getelementptr inbounds ([36 x i8], [36 x i8]* @.str.6, i64 0, i64 0)) #17
  unreachable
}

; Function Attrs: cold inlinehint minsize noreturn nounwind ssp uwtable
define internal void @Object_release_internal.cold.1() #13 {
  tail call void @__assert_rtn(i8* getelementptr inbounds ([24 x i8], [24 x i8]* @__func__.Object_release_internal, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.1, i64 0, i64 0), i32 155, i8* getelementptr inbounds ([36 x i8], [36 x i8]* @.str.6, i64 0, i64 0)) #17
  unreachable
}

; Function Attrs: cold inlinehint minsize noreturn nounwind ssp uwtable
define internal void @Object_autorelease.cold.1() #13 {
  tail call void @__assert_rtn(i8* getelementptr inbounds ([24 x i8], [24 x i8]* @__func__.Object_release_internal, i64 0, i64 0), i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.1, i64 0, i64 0), i32 155, i8* getelementptr inbounds ([36 x i8], [36 x i8]* @.str.6, i64 0, i64 0)) #17
  unreachable
}

attributes #0 = { nounwind ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #1 = { "darwin-stkchk-strong-link" "frame-pointer"="all" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #2 = { inlinehint nounwind ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #3 = { inlinehint mustprogress nounwind ssp uwtable willreturn "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #4 = { inlinehint mustprogress nofree norecurse nosync nounwind readnone ssp uwtable willreturn "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #5 = { inlinehint mustprogress nofree nounwind ssp uwtable willreturn "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #6 = { inlinehint nofree norecurse nounwind ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #7 = { inlinehint mustprogress nofree norecurse nounwind ssp uwtable willreturn "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #8 = { cold noreturn "darwin-stkchk-strong-link" "disable-tail-calls"="true" "frame-pointer"="all" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #9 = { inaccessiblememonly mustprogress nofree nounwind willreturn allocsize(0) "darwin-stkchk-strong-link" "frame-pointer"="all" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #10 = { inaccessiblemem_or_argmemonly mustprogress nounwind willreturn "darwin-stkchk-strong-link" "frame-pointer"="all" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #11 = { nofree nounwind "darwin-stkchk-strong-link" "frame-pointer"="all" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #12 = { nofree nounwind }
attributes #13 = { cold inlinehint minsize noreturn nounwind ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #14 = { nounwind }
attributes #15 = { noinline }
attributes #16 = { allocsize(0) }
attributes #17 = { cold noreturn nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 12, i32 3]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 13.1.6 (clang-1316.0.21.2.5)"}
!6 = !{!"branch_weights", i32 1, i32 2000}
!7 = !{!8}
!8 = distinct !{!8, !9, !"Object_retain: argument 0"}
!9 = distinct !{!9, !"Object_retain"}
!10 = !{!11, !13}
!11 = distinct !{!11, !12, !"Object_release_internal: argument 0"}
!12 = distinct !{!12, !"Object_release_internal"}
!13 = distinct !{!13, !14, !"Object_release: argument 0"}
!14 = distinct !{!14, !"Object_release"}
!15 = !{!"branch_weights", i32 2000, i32 2, i32 2000}
!16 = !{!17}
!17 = distinct !{!17, !18, !"Object_autorelease: argument 0"}
!18 = distinct !{!18, !"Object_autorelease"}
!19 = !{!20, !22, !17}
!20 = distinct !{!20, !21, !"Object_release_internal: argument 0"}
!21 = distinct !{!21, !"Object_release_internal"}
!22 = distinct !{!22, !23, !"Object_release: argument 0"}
!23 = distinct !{!23, !"Object_release"}
!24 = !{!25, !26, i64 0}
!25 = !{!"Object", !26, i64 0, !26, i64 8}
!26 = !{!"omnipotent char", !27, i64 0}
!27 = !{!"Simple C/C++ TBAA"}
!28 = !{!29}
!29 = distinct !{!29, !30, !"Object_release_internal: argument 0"}
!30 = distinct !{!30, !"Object_release_internal"}
!31 = !{!32, !34}
!32 = distinct !{!32, !33, !"Object_release_internal: argument 0"}
!33 = distinct !{!33, !"Object_release_internal"}
!34 = distinct !{!34, !35, !"Object_release: argument 0"}
!35 = distinct !{!35, !"Object_release"}
!36 = !{!37}
!37 = distinct !{!37, !38, !"Object_isReusable: argument 0"}
!38 = distinct !{!38, !"Object_isReusable"}
