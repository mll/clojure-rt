; ModuleID = 'simple_exceptions.cpp'
source_filename = "simple_exceptions.cpp"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx12.0.0"

%class.Exception = type { i8* }

@.str = private unnamed_addr constant [30 x i8] c"Exception requested by caller\00", align 1
@_ZTVN10__cxxabiv119__pointer_type_infoE = external global i8*
@_ZTSP9Exception = linkonce_odr constant [12 x i8] c"P9Exception\00", align 1
@_ZTVN10__cxxabiv117__class_type_infoE = external global i8*
@_ZTS9Exception = linkonce_odr constant [11 x i8] c"9Exception\00", align 1
@_ZTI9Exception = linkonce_odr constant { i8*, i8* } { i8* bitcast (i8** getelementptr inbounds (i8*, i8** @_ZTVN10__cxxabiv117__class_type_infoE, i64 2) to i8*), i8* getelementptr inbounds ([11 x i8], [11 x i8]* @_ZTS9Exception, i32 0, i32 0) }, align 8
@_ZTIP9Exception = linkonce_odr constant { i8*, i8*, i32, i8* } { i8* bitcast (i8** getelementptr inbounds (i8*, i8** @_ZTVN10__cxxabiv119__pointer_type_infoE, i64 2) to i8*), i8* getelementptr inbounds ([12 x i8], [12 x i8]* @_ZTSP9Exception, i32 0, i32 0), i32 0, i8* bitcast ({ i8*, i8* }* @_ZTI9Exception to i8*) }, align 8
@.str.1 = private unnamed_addr constant [4 x i8] c"aaa\00", align 1
@.str.2 = private unnamed_addr constant [7 x i8] c"xxx %s\00", align 1

; Function Attrs: noinline optnone ssp uwtable
define void @_Z3barv() #0 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %1 = alloca i8*, align 8
  %2 = alloca i32, align 4
  %3 = call i8* @__cxa_allocate_exception(i64 8) #9
  %4 = bitcast i8* %3 to %class.Exception**
  %5 = invoke noalias nonnull i8* @_Znwm(i64 8) #10
          to label %6 unwind label %9

6:                                                ; preds = %0
  %7 = bitcast i8* %5 to %class.Exception*
  invoke void @_ZN9ExceptionC1EPKc(%class.Exception* %7, i8* getelementptr inbounds ([30 x i8], [30 x i8]* @.str, i64 0, i64 0))
          to label %8 unwind label %13

8:                                                ; preds = %6
  store %class.Exception* %7, %class.Exception** %4, align 16
  call void @__cxa_throw(i8* %3, i8* bitcast ({ i8*, i8*, i32, i8* }* @_ZTIP9Exception to i8*), i8* null) #11
  unreachable

9:                                                ; preds = %0
  %10 = landingpad { i8*, i32 }
          cleanup
  %11 = extractvalue { i8*, i32 } %10, 0
  store i8* %11, i8** %1, align 8
  %12 = extractvalue { i8*, i32 } %10, 1
  store i32 %12, i32* %2, align 4
  br label %17

13:                                               ; preds = %6
  %14 = landingpad { i8*, i32 }
          cleanup
  %15 = extractvalue { i8*, i32 } %14, 0
  store i8* %15, i8** %1, align 8
  %16 = extractvalue { i8*, i32 } %14, 1
  store i32 %16, i32* %2, align 4
  call void @_ZdlPv(i8* %5) #12
  br label %17

17:                                               ; preds = %13, %9
  call void @__cxa_free_exception(i8* %3) #9
  br label %18

18:                                               ; preds = %17
  %19 = load i8*, i8** %1, align 8
  %20 = load i32, i32* %2, align 4
  %21 = insertvalue { i8*, i32 } undef, i8* %19, 0
  %22 = insertvalue { i8*, i32 } %21, i32 %20, 1
  resume { i8*, i32 } %22
}

declare i8* @__cxa_allocate_exception(i64)

; Function Attrs: nobuiltin allocsize(0)
declare nonnull i8* @_Znwm(i64) #1

declare i32 @__gxx_personality_v0(...)

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr void @_ZN9ExceptionC1EPKc(%class.Exception* %0, i8* %1) unnamed_addr #0 align 2 {
  %3 = alloca %class.Exception*, align 8
  %4 = alloca i8*, align 8
  store %class.Exception* %0, %class.Exception** %3, align 8
  store i8* %1, i8** %4, align 8
  %5 = load %class.Exception*, %class.Exception** %3, align 8
  %6 = load i8*, i8** %4, align 8
  call void @_ZN9ExceptionC2EPKc(%class.Exception* %5, i8* %6)
  ret void
}

; Function Attrs: nobuiltin nounwind
declare void @_ZdlPv(i8*) #2

declare void @__cxa_free_exception(i8*)

declare void @__cxa_throw(i8*, i8*, i8*)

; Function Attrs: noinline optnone ssp uwtable
define void @_Z3foov() #0 {
  call void @_Z3barv()
  ret void
}

; Function Attrs: noinline optnone ssp uwtable
define void @_Z3coov() #0 {
  %1 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.1, i64 0, i64 0))
  ret void
}

declare i32 @printf(i8*, ...) #3

; Function Attrs: noinline optnone ssp uwtable
define void @_Z3doov() #0 {
  call void @_Z3coov()
  ret void
}

; Function Attrs: noinline norecurse optnone ssp uwtable
define i32 @main(i32 %0, i8** %1) #4 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i8**, align 8
  %6 = alloca i8*, align 8
  %7 = alloca i32, align 4
  %8 = alloca %class.Exception, align 8
  store i32 0, i32* %3, align 4
  store i32 %0, i32* %4, align 4
  store i8** %1, i8*** %5, align 8
  invoke void @_Z3doov()
          to label %9 unwind label %11

9:                                                ; preds = %2
  invoke void @_Z3foov()
          to label %10 unwind label %11

10:                                               ; preds = %9
  br label %29

11:                                               ; preds = %9, %2
  %12 = landingpad { i8*, i32 }
          catch i8* bitcast ({ i8*, i8* }* @_ZTI9Exception to i8*)
  %13 = extractvalue { i8*, i32 } %12, 0
  store i8* %13, i8** %6, align 8
  %14 = extractvalue { i8*, i32 } %12, 1
  store i32 %14, i32* %7, align 4
  br label %15

15:                                               ; preds = %11
  %16 = load i32, i32* %7, align 4
  %17 = call i32 @llvm.eh.typeid.for(i8* bitcast ({ i8*, i8* }* @_ZTI9Exception to i8*)) #9
  %18 = icmp eq i32 %16, %17
  br i1 %18, label %19, label %35

19:                                               ; preds = %15
  %20 = load i8*, i8** %6, align 8
  %21 = call i8* @__cxa_begin_catch(i8* %20) #9
  %22 = bitcast i8* %21 to %class.Exception*
  %23 = bitcast %class.Exception* %8 to i8*
  %24 = bitcast %class.Exception* %22 to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 %23, i8* align 8 %24, i64 8, i1 false)
  %25 = invoke i8* @_ZNK9Exception7GetTextEv(%class.Exception* %8)
          to label %26 unwind label %30

26:                                               ; preds = %19
  %27 = invoke i32 (i8*, ...) @printf(i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.2, i64 0, i64 0), i8* %25)
          to label %28 unwind label %30

28:                                               ; preds = %26
  call void @__cxa_end_catch()
  br label %29

29:                                               ; preds = %28, %10
  ret i32 0

30:                                               ; preds = %26, %19
  %31 = landingpad { i8*, i32 }
          cleanup
  %32 = extractvalue { i8*, i32 } %31, 0
  store i8* %32, i8** %6, align 8
  %33 = extractvalue { i8*, i32 } %31, 1
  store i32 %33, i32* %7, align 4
  invoke void @__cxa_end_catch()
          to label %34 unwind label %40

34:                                               ; preds = %30
  br label %35

35:                                               ; preds = %34, %15
  %36 = load i8*, i8** %6, align 8
  %37 = load i32, i32* %7, align 4
  %38 = insertvalue { i8*, i32 } undef, i8* %36, 0
  %39 = insertvalue { i8*, i32 } %38, i32 %37, 1
  resume { i8*, i32 } %39

40:                                               ; preds = %30
  %41 = landingpad { i8*, i32 }
          catch i8* null
  %42 = extractvalue { i8*, i32 } %41, 0
  call void @__clang_call_terminate(i8* %42) #13
  unreachable
}

; Function Attrs: nounwind readnone
declare i32 @llvm.eh.typeid.for(i8*) #5

declare i8* @__cxa_begin_catch(i8*)

; Function Attrs: argmemonly nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #6

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr i8* @_ZNK9Exception7GetTextEv(%class.Exception* %0) #7 align 2 {
  %2 = alloca %class.Exception*, align 8
  store %class.Exception* %0, %class.Exception** %2, align 8
  %3 = load %class.Exception*, %class.Exception** %2, align 8
  %4 = getelementptr inbounds %class.Exception, %class.Exception* %3, i32 0, i32 0
  %5 = load i8*, i8** %4, align 8
  ret i8* %5
}

declare void @__cxa_end_catch()

; Function Attrs: noinline noreturn nounwind
define linkonce_odr hidden void @__clang_call_terminate(i8* %0) #8 {
  %2 = call i8* @__cxa_begin_catch(i8* %0) #9
  call void @_ZSt9terminatev() #13
  unreachable
}

declare void @_ZSt9terminatev()

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN9ExceptionC2EPKc(%class.Exception* %0, i8* %1) unnamed_addr #7 align 2 {
  %3 = alloca %class.Exception*, align 8
  %4 = alloca i8*, align 8
  store %class.Exception* %0, %class.Exception** %3, align 8
  store i8* %1, i8** %4, align 8
  %5 = load %class.Exception*, %class.Exception** %3, align 8
  %6 = getelementptr inbounds %class.Exception, %class.Exception* %5, i32 0, i32 0
  %7 = load i8*, i8** %4, align 8
  store i8* %7, i8** %6, align 8
  ret void
}

attributes #0 = { noinline optnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #1 = { nobuiltin allocsize(0) "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #2 = { nobuiltin nounwind "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #3 = { "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #4 = { noinline norecurse optnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind readnone }
attributes #6 = { argmemonly nofree nounwind willreturn }
attributes #7 = { noinline nounwind optnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #8 = { noinline noreturn nounwind }
attributes #9 = { nounwind }
attributes #10 = { builtin allocsize(0) }
attributes #11 = { noreturn }
attributes #12 = { builtin nounwind }
attributes #13 = { noreturn nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 12, i32 3]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 13.1.6 (clang-1316.0.21.2.5)"}
