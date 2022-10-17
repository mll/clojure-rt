; ModuleID = 'PersistentVectorNode.c'
source_filename = "PersistentVectorNode.c"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx12.0.0"

%struct.PersistentVectorNode = type { i32, i64, [0 x %struct.Object*] }
%struct.Object = type { i32, i64 }
%struct.String = type { %struct.Object*, i8*, i32 }

@.str = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@.str.1 = private unnamed_addr constant [2 x i8] c" \00", align 1

; Function Attrs: nounwind ssp uwtable
define nonnull %struct.PersistentVectorNode* @PersistentVectorNode_allocate(i64 %0, i32 %1) local_unnamed_addr #0 {
  %3 = shl i64 %0, 3
  %4 = add i64 %3, 32
  %5 = tail call i8* @allocate(i64 %4) #4
  %6 = bitcast i8* %5 to %struct.Object*
  %7 = getelementptr inbounds i8, i8* %5, i64 16
  %8 = bitcast i8* %7 to %struct.PersistentVectorNode*
  %9 = bitcast i8* %7 to i32*
  store i32 %1, i32* %9, align 8, !tbaa !6
  %10 = getelementptr inbounds i8, i8* %5, i64 24
  %11 = bitcast i8* %10 to i64*
  store i64 %0, i64* %11, align 8, !tbaa !9
  tail call void @Object_create(%struct.Object* %6, i32 4) #4
  ret %struct.PersistentVectorNode* %8
}

; Function Attrs: argmemonly mustprogress nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

declare i8* @allocate(i64) local_unnamed_addr #2

declare void @Object_create(%struct.Object*, i32) local_unnamed_addr #2

; Function Attrs: argmemonly mustprogress nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind ssp uwtable
define zeroext i8 @PersistentVectorNode_equals(%struct.PersistentVectorNode* nocapture readonly %0, %struct.PersistentVectorNode* nocapture readonly %1) local_unnamed_addr #0 {
  %3 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %0, i64 0, i32 1
  %4 = load i64, i64* %3, align 8, !tbaa !9
  %5 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %1, i64 0, i32 1
  %6 = load i64, i64* %5, align 8, !tbaa !9
  %7 = icmp eq i64 %4, %6
  br i1 %7, label %8, label %22

8:                                                ; preds = %2
  %9 = icmp eq i64 %4, 0
  br i1 %9, label %22, label %13

10:                                               ; preds = %13
  %11 = load i64, i64* %3, align 8, !tbaa !9
  %12 = icmp ugt i64 %11, %21
  br i1 %12, label %13, label %22, !llvm.loop !11

13:                                               ; preds = %8, %10
  %14 = phi i64 [ %21, %10 ], [ 0, %8 ]
  %15 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %0, i64 0, i32 2, i64 %14
  %16 = load %struct.Object*, %struct.Object** %15, align 8, !tbaa !13
  %17 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %1, i64 0, i32 2, i64 %14
  %18 = load %struct.Object*, %struct.Object** %17, align 8, !tbaa !13
  %19 = tail call zeroext i8 @equals(%struct.Object* %16, %struct.Object* %18) #4
  %20 = icmp eq i8 %19, 0
  %21 = add nuw i64 %14, 1
  br i1 %20, label %22, label %10

22:                                               ; preds = %13, %10, %8, %2
  %23 = phi i8 [ 0, %2 ], [ 1, %8 ], [ 0, %13 ], [ 1, %10 ]
  ret i8 %23
}

declare zeroext i8 @equals(%struct.Object*, %struct.Object*) local_unnamed_addr #2

; Function Attrs: nounwind ssp uwtable
define i64 @PersistentVectorNode_hash(%struct.PersistentVectorNode* nocapture readonly %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %0, i64 0, i32 1
  %3 = load i64, i64* %2, align 8, !tbaa !9
  %4 = icmp eq i64 %3, 0
  br i1 %4, label %5, label %7

5:                                                ; preds = %7, %1
  %6 = phi i64 [ 5381, %1 ], [ %14, %7 ]
  ret i64 %6

7:                                                ; preds = %1, %7
  %8 = phi i64 [ %15, %7 ], [ 0, %1 ]
  %9 = phi i64 [ %14, %7 ], [ 5381, %1 ]
  %10 = mul i64 %9, 33
  %11 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %0, i64 0, i32 2, i64 %8
  %12 = load %struct.Object*, %struct.Object** %11, align 8, !tbaa !13
  %13 = tail call i64 @hash(%struct.Object* %12) #4
  %14 = add i64 %13, %10
  %15 = add nuw i64 %8, 1
  %16 = load i64, i64* %2, align 8, !tbaa !9
  %17 = icmp ugt i64 %16, %15
  br i1 %17, label %7, label %5, !llvm.loop !15
}

declare i64 @hash(%struct.Object*) local_unnamed_addr #2

; Function Attrs: nounwind ssp uwtable
define %struct.String* @PersistentVectorNode_toString(%struct.PersistentVectorNode* nocapture readonly %0) local_unnamed_addr #0 {
  %2 = tail call i8* @sdsnew(i8* getelementptr inbounds ([1 x i8], [1 x i8]* @.str, i64 0, i64 0)) #4
  %3 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %0, i64 0, i32 1
  %4 = load i64, i64* %3, align 8, !tbaa !9
  %5 = icmp eq i64 %4, 0
  br i1 %5, label %6, label %9

6:                                                ; preds = %23, %1
  %7 = phi i8* [ %2, %1 ], [ %24, %23 ]
  %8 = tail call %struct.String* @String_create(i8* %7) #4
  ret %struct.String* %8

9:                                                ; preds = %1, %23
  %10 = phi i64 [ %27, %23 ], [ 0, %1 ]
  %11 = phi i8* [ %24, %23 ], [ %2, %1 ]
  %12 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %0, i64 0, i32 2, i64 %10
  %13 = load %struct.Object*, %struct.Object** %12, align 8, !tbaa !13
  %14 = tail call %struct.String* @toString(%struct.Object* %13) #4
  %15 = getelementptr inbounds %struct.String, %struct.String* %14, i64 0, i32 1
  %16 = load i8*, i8** %15, align 8, !tbaa !16
  %17 = tail call i8* @sdscat(i8* %11, i8* %16) #4
  %18 = load i64, i64* %3, align 8, !tbaa !9
  %19 = add i64 %18, -1
  %20 = icmp ugt i64 %19, %10
  br i1 %20, label %21, label %23

21:                                               ; preds = %9
  %22 = tail call i8* @sdscat(i8* %17, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.1, i64 0, i64 0)) #4
  br label %23

23:                                               ; preds = %21, %9
  %24 = phi i8* [ %22, %21 ], [ %17, %9 ]
  %25 = bitcast %struct.String* %14 to i8*
  %26 = tail call zeroext i8 @release(i8* %25) #4
  %27 = add nuw i64 %10, 1
  %28 = load i64, i64* %3, align 8, !tbaa !9
  %29 = icmp ugt i64 %28, %27
  br i1 %29, label %9, label %6, !llvm.loop !19
}

declare i8* @sdsnew(i8*) local_unnamed_addr #2

declare %struct.String* @toString(%struct.Object*) local_unnamed_addr #2

declare i8* @sdscat(i8*, i8*) local_unnamed_addr #2

declare zeroext i8 @release(i8*) local_unnamed_addr #2

declare %struct.String* @String_create(i8*) local_unnamed_addr #2

; Function Attrs: nounwind ssp uwtable
define void @PersistentVectorNode_destroy(%struct.PersistentVectorNode* nocapture readonly %0, i8 zeroext %1) local_unnamed_addr #0 {
  %3 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %0, i64 0, i32 1
  %4 = load i64, i64* %3, align 8, !tbaa !9
  %5 = icmp eq i64 %4, 0
  br i1 %5, label %6, label %7

6:                                                ; preds = %7, %2
  ret void

7:                                                ; preds = %2, %7
  %8 = phi i64 [ %12, %7 ], [ 0, %2 ]
  %9 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %0, i64 0, i32 2, i64 %8
  %10 = load %struct.Object*, %struct.Object** %9, align 8, !tbaa !13
  %11 = tail call zeroext i8 @Object_release(%struct.Object* %10) #4
  %12 = add nuw i64 %8, 1
  %13 = load i64, i64* %3, align 8, !tbaa !9
  %14 = icmp ugt i64 %13, %12
  br i1 %14, label %7, label %6, !llvm.loop !20
}

declare zeroext i8 @Object_release(%struct.Object*) local_unnamed_addr #2

; Function Attrs: nounwind ssp uwtable
define nonnull %struct.PersistentVectorNode* @PersistentVectorNode_replacePath(%struct.PersistentVectorNode* %0, i64 %1, i64 %2, %struct.Object* %3) local_unnamed_addr #0 {
  %5 = lshr i64 %2, %1
  %6 = and i64 %5, 31
  %7 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %0, i64 0, i32 1
  %8 = load i64, i64* %7, align 8, !tbaa !9
  %9 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %0, i64 0, i32 0
  %10 = load i32, i32* %9, align 8, !tbaa !6
  %11 = shl i64 %8, 3
  %12 = add i64 %11, 32
  %13 = tail call i8* @allocate(i64 %12) #4
  %14 = bitcast i8* %13 to %struct.Object*
  %15 = getelementptr inbounds i8, i8* %13, i64 16
  %16 = bitcast i8* %15 to %struct.PersistentVectorNode*
  %17 = bitcast i8* %15 to i32*
  store i32 %10, i32* %17, align 8, !tbaa !6
  %18 = getelementptr inbounds i8, i8* %13, i64 24
  %19 = bitcast i8* %18 to i64*
  store i64 %8, i64* %19, align 8, !tbaa !9
  tail call void @Object_create(%struct.Object* %14, i32 4) #4
  %20 = bitcast %struct.PersistentVectorNode* %0 to i8*
  %21 = load i64, i64* %7, align 8, !tbaa !9
  %22 = shl i64 %21, 3
  %23 = add i64 %22, 16
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* nonnull align 1 %15, i8* align 1 %20, i64 %23, i1 false) #4
  %24 = add i64 %1, -5
  %25 = load i64, i64* %7, align 8, !tbaa !9
  %26 = icmp eq i64 %25, 0
  br i1 %26, label %29, label %27

27:                                               ; preds = %4
  %28 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %16, i64 0, i32 2, i64 %6
  br label %30

29:                                               ; preds = %47, %4
  ret %struct.PersistentVectorNode* %16

30:                                               ; preds = %27, %47
  %31 = phi i64 [ 0, %27 ], [ %48, %47 ]
  %32 = icmp eq i64 %6, %31
  br i1 %32, label %33, label %44

33:                                               ; preds = %30
  %34 = load i32, i32* %9, align 8, !tbaa !6
  %35 = icmp eq i32 %34, 0
  br i1 %35, label %36, label %37

36:                                               ; preds = %33
  tail call void @Object_retain(%struct.Object* %3) #4
  store %struct.Object* %3, %struct.Object** %28, align 8, !tbaa !13
  br label %47

37:                                               ; preds = %33
  %38 = load %struct.Object*, %struct.Object** %28, align 8, !tbaa !13
  %39 = tail call i8* @data(%struct.Object* %38) #4
  %40 = bitcast i8* %39 to %struct.PersistentVectorNode*
  %41 = tail call %struct.PersistentVectorNode* @PersistentVectorNode_replacePath(%struct.PersistentVectorNode* %40, i64 %24, i64 %2, %struct.Object* %3)
  %42 = bitcast %struct.PersistentVectorNode* %41 to i8*
  %43 = tail call %struct.Object* @super(i8* nonnull %42) #4
  store %struct.Object* %43, %struct.Object** %28, align 8, !tbaa !13
  br label %47

44:                                               ; preds = %30
  %45 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %0, i64 0, i32 2, i64 %31
  %46 = load %struct.Object*, %struct.Object** %45, align 8, !tbaa !13
  tail call void @Object_retain(%struct.Object* %46) #4
  br label %47

47:                                               ; preds = %44, %37, %36
  %48 = add nuw i64 %31, 1
  %49 = load i64, i64* %7, align 8, !tbaa !9
  %50 = icmp ugt i64 %49, %48
  br i1 %50, label %30, label %29, !llvm.loop !21
}

declare void @Object_retain(%struct.Object*) local_unnamed_addr #2

declare %struct.Object* @super(i8*) local_unnamed_addr #2

declare i8* @data(%struct.Object*) local_unnamed_addr #2

; Function Attrs: nounwind ssp uwtable
define %struct.PersistentVectorNode* @PersistentVectorNode_pushTail(%struct.PersistentVectorNode* readnone %0, %struct.PersistentVectorNode* %1, %struct.PersistentVectorNode* %2, i32 %3, i8* nocapture %4) local_unnamed_addr #0 {
  %6 = alloca i8, align 1
  %7 = icmp eq %struct.PersistentVectorNode* %1, null
  br i1 %7, label %8, label %10

8:                                                ; preds = %5
  %9 = bitcast %struct.PersistentVectorNode* %2 to i8*
  tail call void @retain(i8* %9) #4
  store i8 0, i8* %4, align 1, !tbaa !6
  br label %148

10:                                               ; preds = %5
  %11 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %1, i64 0, i32 0
  %12 = load i32, i32* %11, align 8, !tbaa !6
  %13 = icmp eq i32 %12, 0
  br i1 %13, label %14, label %30

14:                                               ; preds = %10
  %15 = tail call i8* @allocate(i64 48) #4
  %16 = bitcast i8* %15 to %struct.Object*
  %17 = getelementptr inbounds i8, i8* %15, i64 16
  %18 = bitcast i8* %17 to %struct.PersistentVectorNode*
  %19 = bitcast i8* %17 to i32*
  store i32 1, i32* %19, align 8, !tbaa !6
  %20 = getelementptr inbounds i8, i8* %15, i64 24
  %21 = bitcast i8* %20 to i64*
  store i64 2, i64* %21, align 8, !tbaa !9
  tail call void @Object_create(%struct.Object* %16, i32 4) #4
  %22 = bitcast %struct.PersistentVectorNode* %1 to i8*
  tail call void @retain(i8* nonnull %22) #4
  %23 = bitcast %struct.PersistentVectorNode* %2 to i8*
  tail call void @retain(i8* %23) #4
  %24 = tail call %struct.Object* @super(i8* nonnull %22) #4
  %25 = getelementptr inbounds i8, i8* %15, i64 32
  %26 = bitcast i8* %25 to %struct.Object**
  store %struct.Object* %24, %struct.Object** %26, align 8, !tbaa !13
  %27 = tail call %struct.Object* @super(i8* %23) #4
  %28 = getelementptr inbounds i8, i8* %15, i64 40
  %29 = bitcast i8* %28 to %struct.Object**
  store %struct.Object* %27, %struct.Object** %29, align 8, !tbaa !13
  store i8 0, i8* %4, align 1, !tbaa !6
  br label %148

30:                                               ; preds = %10
  %31 = icmp slt i32 %3, 6
  br i1 %31, label %40, label %32

32:                                               ; preds = %30
  %33 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %1, i64 0, i32 1
  %34 = load i64, i64* %33, align 8, !tbaa !9
  %35 = add i64 %34, -1
  %36 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %1, i64 0, i32 2, i64 %35
  %37 = load %struct.Object*, %struct.Object** %36, align 8, !tbaa !13
  %38 = tail call i8* @data(%struct.Object* %37) #4
  %39 = bitcast i8* %38 to %struct.PersistentVectorNode*
  br label %40

40:                                               ; preds = %30, %32
  %41 = phi %struct.PersistentVectorNode* [ %39, %32 ], [ null, %30 ]
  call void @llvm.lifetime.start.p0i8(i64 1, i8* nonnull %6) #4
  %42 = add nsw i32 %3, -5
  %43 = call %struct.PersistentVectorNode* @PersistentVectorNode_pushTail(%struct.PersistentVectorNode* nonnull %1, %struct.PersistentVectorNode* %41, %struct.PersistentVectorNode* %2, i32 %42, i8* nonnull %6)
  %44 = load i8, i8* %6, align 1, !tbaa !6
  %45 = icmp eq i8 %44, 0
  %46 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %1, i64 0, i32 1
  %47 = load i64, i64* %46, align 8, !tbaa !9
  br i1 %45, label %76, label %48

48:                                               ; preds = %40
  %49 = shl i64 %47, 3
  %50 = add i64 %49, 32
  %51 = tail call i8* @allocate(i64 %50) #4
  %52 = bitcast i8* %51 to %struct.Object*
  %53 = getelementptr inbounds i8, i8* %51, i64 16
  %54 = bitcast i8* %53 to %struct.PersistentVectorNode*
  %55 = bitcast i8* %53 to i32*
  store i32 1, i32* %55, align 8, !tbaa !6
  %56 = getelementptr inbounds i8, i8* %51, i64 24
  %57 = bitcast i8* %56 to i64*
  store i64 %47, i64* %57, align 8, !tbaa !9
  tail call void @Object_create(%struct.Object* %52, i32 4) #4
  %58 = bitcast %struct.PersistentVectorNode* %1 to i8*
  %59 = load i64, i64* %46, align 8, !tbaa !9
  %60 = shl i64 %59, 3
  %61 = add i64 %60, 16
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* nonnull align 1 %53, i8* nonnull align 1 %58, i64 %61, i1 false) #4
  %62 = bitcast %struct.PersistentVectorNode* %43 to i8*
  %63 = tail call %struct.Object* @super(i8* %62) #4
  %64 = load i64, i64* %57, align 8, !tbaa !9
  %65 = add i64 %64, -1
  %66 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %54, i64 0, i32 2, i64 %65
  store %struct.Object* %63, %struct.Object** %66, align 8, !tbaa !13
  %67 = icmp eq i64 %64, 1
  br i1 %67, label %145, label %68

68:                                               ; preds = %48, %68
  %69 = phi i64 [ %72, %68 ], [ 0, %48 ]
  %70 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %54, i64 0, i32 2, i64 %69
  %71 = load %struct.Object*, %struct.Object** %70, align 8, !tbaa !13
  tail call void @Object_retain(%struct.Object* %71) #4
  %72 = add nuw i64 %69, 1
  %73 = load i64, i64* %57, align 8, !tbaa !9
  %74 = add i64 %73, -1
  %75 = icmp ugt i64 %74, %72
  br i1 %75, label %68, label %145, !llvm.loop !22

76:                                               ; preds = %40
  %77 = icmp ult i64 %47, 32
  br i1 %77, label %78, label %108

78:                                               ; preds = %76
  %79 = add nuw nsw i64 %47, 1
  %80 = shl nuw nsw i64 %79, 3
  %81 = add nuw nsw i64 %80, 32
  %82 = tail call i8* @allocate(i64 %81) #4
  %83 = bitcast i8* %82 to %struct.Object*
  %84 = getelementptr inbounds i8, i8* %82, i64 16
  %85 = bitcast i8* %84 to %struct.PersistentVectorNode*
  %86 = bitcast i8* %84 to i32*
  store i32 1, i32* %86, align 8, !tbaa !6
  %87 = getelementptr inbounds i8, i8* %82, i64 24
  %88 = bitcast i8* %87 to i64*
  store i64 %79, i64* %88, align 8, !tbaa !9
  tail call void @Object_create(%struct.Object* %83, i32 4) #4
  %89 = bitcast %struct.PersistentVectorNode* %1 to i8*
  %90 = load i64, i64* %46, align 8, !tbaa !9
  %91 = shl i64 %90, 3
  %92 = add i64 %91, 16
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* nonnull align 1 %84, i8* nonnull align 1 %89, i64 %92, i1 false) #4
  %93 = bitcast %struct.PersistentVectorNode* %43 to i8*
  %94 = tail call %struct.Object* @super(i8* %93) #4
  %95 = load i64, i64* %46, align 8, !tbaa !9
  %96 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %85, i64 0, i32 2, i64 %95
  store %struct.Object* %94, %struct.Object** %96, align 8, !tbaa !13
  %97 = load i64, i64* %88, align 8, !tbaa !9
  %98 = add i64 %97, 1
  store i64 %98, i64* %88, align 8, !tbaa !9
  %99 = icmp eq i64 %97, 0
  br i1 %99, label %145, label %100

100:                                              ; preds = %78, %100
  %101 = phi i64 [ %104, %100 ], [ 0, %78 ]
  %102 = getelementptr inbounds %struct.PersistentVectorNode, %struct.PersistentVectorNode* %85, i64 0, i32 2, i64 %101
  %103 = load %struct.Object*, %struct.Object** %102, align 8, !tbaa !13
  tail call void @Object_retain(%struct.Object* %103) #4
  %104 = add nuw i64 %101, 1
  %105 = load i64, i64* %88, align 8, !tbaa !9
  %106 = add i64 %105, -1
  %107 = icmp ugt i64 %106, %104
  br i1 %107, label %100, label %145, !llvm.loop !23

108:                                              ; preds = %76
  %109 = icmp eq %struct.PersistentVectorNode* %0, null
  br i1 %109, label %110, label %130

110:                                              ; preds = %108
  %111 = tail call i8* @allocate(i64 48) #4
  %112 = bitcast i8* %111 to %struct.Object*
  %113 = getelementptr inbounds i8, i8* %111, i64 16
  %114 = bitcast i8* %113 to i32*
  store i32 1, i32* %114, align 8, !tbaa !6
  %115 = getelementptr inbounds i8, i8* %111, i64 24
  %116 = bitcast i8* %115 to i64*
  store i64 2, i64* %116, align 8, !tbaa !9
  tail call void @Object_create(%struct.Object* %112, i32 4) #4
  %117 = bitcast %struct.PersistentVectorNode* %1 to i8*
  tail call void @retain(i8* nonnull %117) #4
  %118 = tail call %struct.Object* @super(i8* nonnull %117) #4
  %119 = getelementptr inbounds i8, i8* %111, i64 32
  %120 = bitcast i8* %119 to %struct.Object**
  store %struct.Object* %118, %struct.Object** %120, align 8, !tbaa !13
  %121 = tail call i8* @allocate(i64 40) #4
  %122 = bitcast i8* %121 to %struct.Object*
  %123 = getelementptr inbounds i8, i8* %121, i64 16
  %124 = bitcast i8* %123 to i32*
  store i32 1, i32* %124, align 8, !tbaa !6
  %125 = getelementptr inbounds i8, i8* %121, i64 24
  %126 = bitcast i8* %125 to i64*
  store i64 1, i64* %126, align 8, !tbaa !9
  tail call void @Object_create(%struct.Object* %122, i32 4) #4
  %127 = tail call %struct.Object* @super(i8* nonnull %123) #4
  %128 = getelementptr inbounds i8, i8* %111, i64 40
  %129 = bitcast i8* %128 to %struct.Object**
  store %struct.Object* %127, %struct.Object** %129, align 8, !tbaa !13
  br label %137

130:                                              ; preds = %108
  %131 = tail call i8* @allocate(i64 40) #4
  %132 = bitcast i8* %131 to %struct.Object*
  %133 = getelementptr inbounds i8, i8* %131, i64 16
  %134 = bitcast i8* %133 to i32*
  store i32 1, i32* %134, align 8, !tbaa !6
  %135 = getelementptr inbounds i8, i8* %131, i64 24
  %136 = bitcast i8* %135 to i64*
  store i64 1, i64* %136, align 8, !tbaa !9
  tail call void @Object_create(%struct.Object* %132, i32 4) #4
  br label %137

137:                                              ; preds = %110, %130
  %138 = phi i8* [ %131, %130 ], [ %121, %110 ]
  %139 = phi i8* [ %133, %130 ], [ %113, %110 ]
  %140 = bitcast i8* %139 to %struct.PersistentVectorNode*
  %141 = bitcast %struct.PersistentVectorNode* %43 to i8*
  %142 = tail call %struct.Object* @super(i8* %141) #4
  %143 = getelementptr inbounds i8, i8* %138, i64 32
  %144 = bitcast i8* %143 to %struct.Object**
  store %struct.Object* %142, %struct.Object** %144, align 8, !tbaa !13
  br label %145

145:                                              ; preds = %68, %100, %137, %78, %48
  %146 = phi i8 [ 1, %48 ], [ 1, %78 ], [ 0, %137 ], [ 1, %100 ], [ 1, %68 ]
  %147 = phi %struct.PersistentVectorNode* [ %54, %48 ], [ %85, %78 ], [ %140, %137 ], [ %85, %100 ], [ %54, %68 ]
  store i8 %146, i8* %4, align 1, !tbaa !6
  call void @llvm.lifetime.end.p0i8(i64 1, i8* nonnull %6) #4
  br label %148

148:                                              ; preds = %145, %14, %8
  %149 = phi %struct.PersistentVectorNode* [ %2, %8 ], [ %18, %14 ], [ %147, %145 ]
  ret %struct.PersistentVectorNode* %149
}

declare void @retain(i8*) local_unnamed_addr #2

; Function Attrs: argmemonly nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #3

attributes #0 = { nounwind ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #1 = { argmemonly mustprogress nofree nosync nounwind willreturn }
attributes #2 = { "darwin-stkchk-strong-link" "frame-pointer"="all" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" "unsafe-fp-math"="true" }
attributes #3 = { argmemonly nofree nounwind willreturn }
attributes #4 = { nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 12, i32 3]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 13.1.6 (clang-1316.0.21.2.5)"}
!6 = !{!7, !7, i64 0}
!7 = !{!"omnipotent char", !8, i64 0}
!8 = !{!"Simple C/C++ TBAA"}
!9 = !{!10, !10, i64 0}
!10 = !{!"long long", !7, i64 0}
!11 = distinct !{!11, !12}
!12 = !{!"llvm.loop.mustprogress"}
!13 = !{!14, !14, i64 0}
!14 = !{!"any pointer", !7, i64 0}
!15 = distinct !{!15, !12}
!16 = !{!17, !14, i64 8}
!17 = !{!"String", !14, i64 0, !14, i64 8, !18, i64 16}
!18 = !{!"int", !7, i64 0}
!19 = distinct !{!19, !12}
!20 = distinct !{!20, !12}
!21 = distinct !{!21, !12}
!22 = distinct !{!22, !12}
!23 = distinct !{!23, !12}
