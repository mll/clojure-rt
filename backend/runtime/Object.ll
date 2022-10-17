; ModuleID = 'Object.c'
source_filename = "Object.c"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx12.0.0"

%struct.pool = type { i64, i64, i64, i64, %struct.poolFreed*, i64, i8** }
%struct.poolFreed = type { %struct.poolFreed* }
%struct.Object = type { i32, i64 }
%struct.Integer = type { i32 }
%struct.String = type { %struct.Object*, i8*, i32 }
%struct.PersistentList = type { %struct.Object*, %struct.Object*, %struct.PersistentList*, i64 }
%struct.PersistentVector = type { i64, i64, %struct.PersistentVectorNode*, %struct.PersistentVectorNode* }
%struct.PersistentVectorNode = type { i32, i64, [0 x %struct.Object*] }
%struct.Double = type { double }

@allocationCount = common local_unnamed_addr global [5 x i32] zeroinitializer, align 16
@globalPool1 = common global %struct.pool zeroinitializer, align 8
@globalPool2 = common global %struct.pool zeroinitializer, align 8
@globalPool3 = common global %struct.pool zeroinitializer, align 8

; Function Attrs: nounwind ssp uwtable
define void @initialise_memory() local_unnamed_addr #0 {
  tail call void @llvm.memset.p0i8.i64(i8* noundef nonnull align 16 dereferenceable(20) bitcast ([5 x i32]* @allocationCount to i8*), i8 0, i64 20, i1 false)
  tail call void @poolInitialize(%struct.pool* nonnull @globalPool1, i64 320, i64 100000) #11
  tail call void @poolInitialize(%struct.pool* nonnull @globalPool2, i64 128, i64 100000) #11
  tail call void @poolInitialize(%struct.pool* nonnull @globalPool3, i64 64, i64 100000) #11
  tail call void (...) @PersistentVector_initialise() #11
  ret void
}

; Function Attrs: argmemonly mustprogress nofree nounwind willreturn writeonly
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg) #1

declare void @poolInitialize(%struct.pool*, i64, i64) local_unnamed_addr #2

declare void @PersistentVector_initialise(...) local_unnamed_addr #2

; Function Attrs: nofree norecurse nosync nounwind readonly ssp uwtable
define zeroext i8 @poolFreeCheck(i8* readnone %0, %struct.pool* nocapture readonly %1) local_unnamed_addr #3 {
  %3 = getelementptr inbounds %struct.pool, %struct.pool* %1, i64 0, i32 5
  %4 = load i64, i64* %3, align 8, !tbaa !6
  %5 = icmp eq i64 %4, 0
  br i1 %5, label %28, label %6

6:                                                ; preds = %2
  %7 = getelementptr inbounds %struct.pool, %struct.pool* %1, i64 0, i32 0
  %8 = getelementptr inbounds %struct.pool, %struct.pool* %1, i64 0, i32 1
  %9 = getelementptr inbounds %struct.pool, %struct.pool* %1, i64 0, i32 6
  %10 = load i8**, i8*** %9, align 8, !tbaa !12
  %11 = load i64, i64* %8, align 8, !tbaa !13
  %12 = load i64, i64* %7, align 8, !tbaa !14
  %13 = mul i64 %12, %11
  br label %14

14:                                               ; preds = %6, %25
  %15 = phi i64 [ 0, %6 ], [ %26, %25 ]
  %16 = getelementptr inbounds i8*, i8** %10, i64 %15
  %17 = load i8*, i8** %16, align 8, !tbaa !15
  %18 = getelementptr inbounds i8, i8* %17, i64 %13
  %19 = icmp eq i8* %17, null
  %20 = icmp ule i8* %17, %0
  %21 = icmp ugt i8* %18, %0
  %22 = select i1 %20, i1 %21, i1 false
  %23 = zext i1 %22 to i32
  %24 = select i1 %19, i32 4, i32 %23
  switch i32 %24, label %28 [
    i32 0, label %25
    i32 4, label %25
  ]

25:                                               ; preds = %14, %14
  %26 = add nuw i64 %15, 1
  %27 = icmp eq i64 %26, %4
  br i1 %27, label %28, label %14, !llvm.loop !16

28:                                               ; preds = %14, %25, %2
  %29 = phi i8 [ 0, %2 ], [ 0, %25 ], [ 1, %14 ]
  ret i8 %29
}

; Function Attrs: mustprogress nofree nounwind ssp uwtable willreturn
define noalias i8* @allocate(i64 %0) local_unnamed_addr #4 {
  %2 = tail call i8* @malloc(i64 %0) #12
  ret i8* %2
}

; Function Attrs: inaccessiblememonly mustprogress nofree nounwind willreturn allocsize(0)
declare noalias noundef i8* @malloc(i64 noundef) local_unnamed_addr #5

; Function Attrs: mustprogress nounwind ssp uwtable willreturn
define void @deallocate(i8* nocapture %0) local_unnamed_addr #6 {
  tail call void @free(i8* %0)
  ret void
}

; Function Attrs: inaccessiblemem_or_argmemonly mustprogress nounwind willreturn
declare void @free(i8* nocapture noundef) local_unnamed_addr #7

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone ssp uwtable willreturn
define nonnull i8* @data(%struct.Object* readnone %0) local_unnamed_addr #8 {
  %2 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %3 = bitcast %struct.Object* %2 to i8*
  ret i8* %3
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind readnone ssp uwtable willreturn
define %struct.Object* @super(i8* readnone %0) local_unnamed_addr #8 {
  %2 = getelementptr i8, i8* %0, i64 -16
  %3 = bitcast i8* %2 to %struct.Object*
  ret %struct.Object* %3
}

; Function Attrs: nofree norecurse nounwind ssp uwtable
define void @Object_create(%struct.Object* %0, i32 %1) local_unnamed_addr #9 {
  %3 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 1
  store atomic volatile i64 1, i64* %3 monotonic, align 8
  %4 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 0
  store i32 %1, i32* %4, align 8, !tbaa !18
  ret void
}

; Function Attrs: nounwind ssp uwtable
define zeroext i8 @equals(%struct.Object* %0, %struct.Object* %1) local_unnamed_addr #0 {
  %3 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %4 = getelementptr inbounds %struct.Object, %struct.Object* %1, i64 1
  %5 = icmp eq %struct.Object* %0, %1
  br i1 %5, label %37, label %6

6:                                                ; preds = %2
  %7 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 0
  %8 = load i32, i32* %7, align 8, !tbaa !18
  %9 = getelementptr inbounds %struct.Object, %struct.Object* %1, i64 0, i32 0
  %10 = load i32, i32* %9, align 8, !tbaa !18
  %11 = icmp eq i32 %8, %10
  br i1 %11, label %12, label %37

12:                                               ; preds = %6
  switch i32 %8, label %37 [
    i32 0, label %13
    i32 1, label %17
    i32 2, label %21
    i32 3, label %25
    i32 4, label %29
    i32 5, label %33
  ]

13:                                               ; preds = %12
  %14 = bitcast %struct.Object* %3 to %struct.Integer*
  %15 = bitcast %struct.Object* %4 to %struct.Integer*
  %16 = tail call zeroext i8 @Integer_equals(%struct.Integer* nonnull %14, %struct.Integer* nonnull %15) #11
  br label %37

17:                                               ; preds = %12
  %18 = bitcast %struct.Object* %3 to %struct.String*
  %19 = bitcast %struct.Object* %4 to %struct.String*
  %20 = tail call zeroext i8 @String_equals(%struct.String* nonnull %18, %struct.String* nonnull %19) #11
  br label %37

21:                                               ; preds = %12
  %22 = bitcast %struct.Object* %3 to %struct.PersistentList*
  %23 = bitcast %struct.Object* %4 to %struct.PersistentList*
  %24 = tail call zeroext i8 @PersistentList_equals(%struct.PersistentList* nonnull %22, %struct.PersistentList* nonnull %23) #11
  br label %37

25:                                               ; preds = %12
  %26 = bitcast %struct.Object* %3 to %struct.PersistentVector*
  %27 = bitcast %struct.Object* %4 to %struct.PersistentVector*
  %28 = tail call zeroext i8 @PersistentVector_equals(%struct.PersistentVector* nonnull %26, %struct.PersistentVector* nonnull %27) #11
  br label %37

29:                                               ; preds = %12
  %30 = bitcast %struct.Object* %3 to %struct.PersistentVectorNode*
  %31 = bitcast %struct.Object* %4 to %struct.PersistentVectorNode*
  %32 = tail call zeroext i8 @PersistentVectorNode_equals(%struct.PersistentVectorNode* nonnull %30, %struct.PersistentVectorNode* nonnull %31) #11
  br label %37

33:                                               ; preds = %12
  %34 = bitcast %struct.Object* %3 to %struct.Double*
  %35 = bitcast %struct.Object* %4 to %struct.Double*
  %36 = tail call zeroext i8 @Double_equals(%struct.Double* nonnull %34, %struct.Double* nonnull %35) #11
  br label %37

37:                                               ; preds = %12, %6, %2, %33, %29, %25, %21, %17, %13
  %38 = phi i8 [ %36, %33 ], [ %32, %29 ], [ %28, %25 ], [ %24, %21 ], [ %20, %17 ], [ %16, %13 ], [ 1, %2 ], [ 0, %6 ], [ undef, %12 ]
  ret i8 %38
}

declare zeroext i8 @Integer_equals(%struct.Integer*, %struct.Integer*) local_unnamed_addr #2

declare zeroext i8 @String_equals(%struct.String*, %struct.String*) local_unnamed_addr #2

declare zeroext i8 @PersistentList_equals(%struct.PersistentList*, %struct.PersistentList*) local_unnamed_addr #2

declare zeroext i8 @PersistentVector_equals(%struct.PersistentVector*, %struct.PersistentVector*) local_unnamed_addr #2

declare zeroext i8 @PersistentVectorNode_equals(%struct.PersistentVectorNode*, %struct.PersistentVectorNode*) local_unnamed_addr #2

declare zeroext i8 @Double_equals(%struct.Double*, %struct.Double*) local_unnamed_addr #2

; Function Attrs: nounwind ssp uwtable
define i64 @hash(%struct.Object* %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 0
  %3 = load i32, i32* %2, align 8, !tbaa !18
  switch i32 %3, label %28 [
    i32 0, label %4
    i32 1, label %8
    i32 2, label %12
    i32 3, label %16
    i32 4, label %20
    i32 5, label %24
  ]

4:                                                ; preds = %1
  %5 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %6 = bitcast %struct.Object* %5 to %struct.Integer*
  %7 = tail call i64 @Integer_hash(%struct.Integer* nonnull %6) #11
  br label %28

8:                                                ; preds = %1
  %9 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %10 = bitcast %struct.Object* %9 to %struct.String*
  %11 = tail call i64 @String_hash(%struct.String* nonnull %10) #11
  br label %28

12:                                               ; preds = %1
  %13 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %14 = bitcast %struct.Object* %13 to %struct.PersistentList*
  %15 = tail call i64 @PersistentList_hash(%struct.PersistentList* nonnull %14) #11
  br label %28

16:                                               ; preds = %1
  %17 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %18 = bitcast %struct.Object* %17 to %struct.PersistentVector*
  %19 = tail call i64 @PersistentVector_hash(%struct.PersistentVector* nonnull %18) #11
  br label %28

20:                                               ; preds = %1
  %21 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %22 = bitcast %struct.Object* %21 to %struct.PersistentVector*
  %23 = tail call i64 @PersistentVector_hash(%struct.PersistentVector* nonnull %22) #11
  br label %28

24:                                               ; preds = %1
  %25 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %26 = bitcast %struct.Object* %25 to %struct.Double*
  %27 = tail call i64 @Double_hash(%struct.Double* nonnull %26) #11
  br label %28

28:                                               ; preds = %4, %8, %12, %16, %20, %24, %1
  %29 = phi i64 [ undef, %1 ], [ %27, %24 ], [ %23, %20 ], [ %19, %16 ], [ %15, %12 ], [ %11, %8 ], [ %7, %4 ]
  ret i64 %29
}

declare i64 @Integer_hash(%struct.Integer*) local_unnamed_addr #2

declare i64 @String_hash(%struct.String*) local_unnamed_addr #2

declare i64 @PersistentList_hash(%struct.PersistentList*) local_unnamed_addr #2

declare i64 @PersistentVector_hash(%struct.PersistentVector*) local_unnamed_addr #2

declare i64 @Double_hash(%struct.Double*) local_unnamed_addr #2

; Function Attrs: nounwind ssp uwtable
define %struct.String* @toString(%struct.Object* %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 0
  %3 = load i32, i32* %2, align 8, !tbaa !18
  switch i32 %3, label %28 [
    i32 0, label %4
    i32 1, label %8
    i32 2, label %12
    i32 3, label %16
    i32 4, label %20
    i32 5, label %24
  ]

4:                                                ; preds = %1
  %5 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %6 = bitcast %struct.Object* %5 to %struct.Integer*
  %7 = tail call %struct.String* @Integer_toString(%struct.Integer* nonnull %6) #11
  br label %28

8:                                                ; preds = %1
  %9 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %10 = bitcast %struct.Object* %9 to %struct.String*
  %11 = tail call %struct.String* @String_toString(%struct.String* nonnull %10) #11
  br label %28

12:                                               ; preds = %1
  %13 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %14 = bitcast %struct.Object* %13 to %struct.PersistentList*
  %15 = tail call %struct.String* @PersistentList_toString(%struct.PersistentList* nonnull %14) #11
  br label %28

16:                                               ; preds = %1
  %17 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %18 = bitcast %struct.Object* %17 to %struct.PersistentVector*
  %19 = tail call %struct.String* @PersistentVector_toString(%struct.PersistentVector* nonnull %18) #11
  br label %28

20:                                               ; preds = %1
  %21 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %22 = bitcast %struct.Object* %21 to %struct.PersistentVectorNode*
  %23 = tail call %struct.String* @PersistentVectorNode_toString(%struct.PersistentVectorNode* nonnull %22) #11
  br label %28

24:                                               ; preds = %1
  %25 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %26 = bitcast %struct.Object* %25 to %struct.Double*
  %27 = tail call %struct.String* @Double_toString(%struct.Double* nonnull %26) #11
  br label %28

28:                                               ; preds = %4, %8, %12, %16, %20, %24, %1
  %29 = phi %struct.String* [ undef, %1 ], [ %27, %24 ], [ %23, %20 ], [ %19, %16 ], [ %15, %12 ], [ %11, %8 ], [ %7, %4 ]
  ret %struct.String* %29
}

declare %struct.String* @Integer_toString(%struct.Integer*) local_unnamed_addr #2

declare %struct.String* @String_toString(%struct.String*) local_unnamed_addr #2

declare %struct.String* @PersistentList_toString(%struct.PersistentList*) local_unnamed_addr #2

declare %struct.String* @PersistentVector_toString(%struct.PersistentVector*) local_unnamed_addr #2

declare %struct.String* @PersistentVectorNode_toString(%struct.PersistentVectorNode*) local_unnamed_addr #2

declare %struct.String* @Double_toString(%struct.Double*) local_unnamed_addr #2

; Function Attrs: mustprogress nofree norecurse nounwind ssp uwtable willreturn
define void @retain(i8* %0) local_unnamed_addr #10 {
  %2 = getelementptr i8, i8* %0, i64 -8
  %3 = bitcast i8* %2 to i64*
  %4 = atomicrmw volatile add i64* %3, i64 1 seq_cst, align 8
  ret void
}

; Function Attrs: mustprogress nofree norecurse nounwind ssp uwtable willreturn
define void @Object_retain(%struct.Object* %0) local_unnamed_addr #10 {
  %2 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 1
  %3 = atomicrmw volatile add i64* %2, i64 1 seq_cst, align 8
  ret void
}

; Function Attrs: nounwind ssp uwtable
define zeroext i8 @release(i8* %0) local_unnamed_addr #0 {
  %2 = getelementptr i8, i8* %0, i64 -16
  %3 = bitcast i8* %2 to %struct.Object*
  %4 = tail call zeroext i8 @Object_release_internal(%struct.Object* %3, i8 zeroext 1) #11
  ret i8 %4
}

; Function Attrs: nounwind ssp uwtable
define zeroext i8 @Object_release(%struct.Object* %0) local_unnamed_addr #0 {
  %2 = tail call zeroext i8 @Object_release_internal(%struct.Object* %0, i8 zeroext 1)
  ret i8 %2
}

; Function Attrs: nounwind ssp uwtable
define zeroext i8 @Object_release_internal(%struct.Object* %0, i8 zeroext %1) local_unnamed_addr #0 {
  %3 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 1
  %4 = atomicrmw volatile sub i64* %3, i64 1 seq_cst, align 8
  %5 = icmp eq i64 %4, 1
  br i1 %5, label %6, label %29

6:                                                ; preds = %2
  %7 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 0, i32 0
  %8 = load i32, i32* %7, align 8, !tbaa !18
  switch i32 %8, label %27 [
    i32 0, label %9
    i32 1, label %12
    i32 2, label %15
    i32 3, label %18
    i32 4, label %21
    i32 5, label %24
  ]

9:                                                ; preds = %6
  %10 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %11 = bitcast %struct.Object* %10 to %struct.Integer*
  tail call void @Integer_destroy(%struct.Integer* nonnull %11) #11
  br label %27

12:                                               ; preds = %6
  %13 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %14 = bitcast %struct.Object* %13 to %struct.String*
  tail call void @String_destroy(%struct.String* nonnull %14) #11
  br label %27

15:                                               ; preds = %6
  %16 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %17 = bitcast %struct.Object* %16 to %struct.PersistentList*
  tail call void @PersistentList_destroy(%struct.PersistentList* nonnull %17, i8 zeroext %1) #11
  br label %27

18:                                               ; preds = %6
  %19 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %20 = bitcast %struct.Object* %19 to %struct.PersistentVector*
  tail call void @PersistentVector_destroy(%struct.PersistentVector* nonnull %20, i8 zeroext %1) #11
  br label %27

21:                                               ; preds = %6
  %22 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %23 = bitcast %struct.Object* %22 to %struct.PersistentVectorNode*
  tail call void @PersistentVectorNode_destroy(%struct.PersistentVectorNode* nonnull %23, i8 zeroext %1) #11
  br label %27

24:                                               ; preds = %6
  %25 = getelementptr inbounds %struct.Object, %struct.Object* %0, i64 1
  %26 = bitcast %struct.Object* %25 to %struct.Double*
  tail call void @Double_destroy(%struct.Double* nonnull %26) #11
  br label %27

27:                                               ; preds = %6, %24, %21, %18, %15, %12, %9
  %28 = bitcast %struct.Object* %0 to i8*
  tail call void @free(i8* %28) #11
  br label %29

29:                                               ; preds = %2, %27
  %30 = phi i8 [ 1, %27 ], [ 0, %2 ]
  ret i8 %30
}

declare void @Integer_destroy(%struct.Integer*) local_unnamed_addr #2

declare void @String_destroy(%struct.String*) local_unnamed_addr #2

declare void @PersistentList_destroy(%struct.PersistentList*, i8 zeroext) local_unnamed_addr #2

declare void @PersistentVector_destroy(%struct.PersistentVector*, i8 zeroext) local_unnamed_addr #2

declare void @PersistentVectorNode_destroy(%struct.PersistentVectorNode*, i8 zeroext) local_unnamed_addr #2

declare void @Double_destroy(%struct.Double*) local_unnamed_addr #2

attributes #0 = { nounwind ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #1 = { argmemonly mustprogress nofree nounwind willreturn writeonly }
attributes #2 = { "darwin-stkchk-strong-link" "frame-pointer"="all" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #3 = { nofree norecurse nosync nounwind readonly ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #4 = { mustprogress nofree nounwind ssp uwtable willreturn "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #5 = { inaccessiblememonly mustprogress nofree nounwind willreturn allocsize(0) "darwin-stkchk-strong-link" "frame-pointer"="all" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #6 = { mustprogress nounwind ssp uwtable willreturn "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #7 = { inaccessiblemem_or_argmemonly mustprogress nounwind willreturn "darwin-stkchk-strong-link" "frame-pointer"="all" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #8 = { mustprogress nofree norecurse nosync nounwind readnone ssp uwtable willreturn "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #9 = { nofree norecurse nounwind ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #10 = { mustprogress nofree norecurse nounwind ssp uwtable willreturn "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #11 = { nounwind }
attributes #12 = { allocsize(0) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 12, i32 3]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 13.1.6 (clang-1316.0.21.2.5)"}
!6 = !{!7, !8, i64 40}
!7 = !{!"", !8, i64 0, !8, i64 8, !8, i64 16, !8, i64 24, !11, i64 32, !8, i64 40, !11, i64 48}
!8 = !{!"long long", !9, i64 0}
!9 = !{!"omnipotent char", !10, i64 0}
!10 = !{!"Simple C/C++ TBAA"}
!11 = !{!"any pointer", !9, i64 0}
!12 = !{!7, !11, i64 48}
!13 = !{!7, !8, i64 8}
!14 = !{!7, !8, i64 0}
!15 = !{!11, !11, i64 0}
!16 = distinct !{!16, !17}
!17 = !{!"llvm.loop.mustprogress"}
!18 = !{!19, !9, i64 0}
!19 = !{!"Object", !9, i64 0, !9, i64 8}
