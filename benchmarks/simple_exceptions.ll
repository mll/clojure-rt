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

; Function Attrs: noinline optnone ssp uwtable
define void @_Z3barv() #0 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %1 = alloca i8*, align 8
  %2 = alloca i32, align 4
  %3 = call i8* @__cxa_allocate_exception(i64 8) #5
  %4 = bitcast i8* %3 to %class.Exception**
  %5 = invoke noalias nonnull i8* @_Znwm(i64 8) #6
          to label %6 unwind label %9

6:                                                ; preds = %0
  %7 = bitcast i8* %5 to %class.Exception*
  invoke void @_ZN9ExceptionC1EPKc(%class.Exception* %7, i8* getelementptr inbounds ([30 x i8], [30 x i8]* @.str, i64 0, i64 0))
          to label %8 unwind label %13

8:                                                ; preds = %6
  store %class.Exception* %7, %class.Exception** %4, align 16
  call void @__cxa_throw(i8* %3, i8* bitcast ({ i8*, i8*, i32, i8* }* @_ZTIP9Exception to i8*), i8* null) #7
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
  call void @_ZdlPv(i8* %5) #8
  br label %17

17:                                               ; preds = %13, %9
  call void @__cxa_free_exception(i8* %3) #5
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

; Function Attrs: noinline norecurse optnone ssp uwtable
define i32 @main(i32 %0, i8** %1) #3 {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i8**, align 8
  store i32 0, i32* %3, align 4
  store i32 %0, i32* %4, align 4
  store i8** %1, i8*** %5, align 8
  call void @_Z3barv()
  ret i32 0
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN9ExceptionC2EPKc(%class.Exception* %0, i8* %1) unnamed_addr #4 align 2 {
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
attributes #3 = { noinline norecurse optnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #4 = { noinline nounwind optnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind }
attributes #6 = { builtin allocsize(0) }
attributes #7 = { noreturn }
attributes #8 = { builtin nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 12, i32 3]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 13.1.6 (clang-1316.0.21.2.5)"}
