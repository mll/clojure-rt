; ModuleID = 'exceptions.cpp'
source_filename = "exceptions.cpp"
target datalayout = "e-m:o-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx12.0.0"

%class.Foo = type { i32 }
%class.Exception = type { %class.Object, i8* }
%class.Object = type { i32 (...)** }

@.str = private unnamed_addr constant [30 x i8] c"Exception requested by caller\00", align 1
@_ZTVN10__cxxabiv119__pointer_type_infoE = external global i8*
@_ZTSP9Exception = linkonce_odr constant [12 x i8] c"P9Exception\00", align 1
@_ZTVN10__cxxabiv120__si_class_type_infoE = external global i8*
@_ZTS9Exception = linkonce_odr constant [11 x i8] c"9Exception\00", align 1
@_ZTVN10__cxxabiv117__class_type_infoE = external global i8*
@_ZTS6Object = linkonce_odr constant [8 x i8] c"6Object\00", align 1
@_ZTI6Object = linkonce_odr constant { i8*, i8* } { i8* bitcast (i8** getelementptr inbounds (i8*, i8** @_ZTVN10__cxxabiv117__class_type_infoE, i64 2) to i8*), i8* getelementptr inbounds ([8 x i8], [8 x i8]* @_ZTS6Object, i32 0, i32 0) }, align 8
@_ZTI9Exception = linkonce_odr constant { i8*, i8*, i8* } { i8* bitcast (i8** getelementptr inbounds (i8*, i8** @_ZTVN10__cxxabiv120__si_class_type_infoE, i64 2) to i8*), i8* getelementptr inbounds ([11 x i8], [11 x i8]* @_ZTS9Exception, i32 0, i32 0), i8* bitcast ({ i8*, i8* }* @_ZTI6Object to i8*) }, align 8
@_ZTIP9Exception = linkonce_odr constant { i8*, i8*, i32, i8* } { i8* bitcast (i8** getelementptr inbounds (i8*, i8** @_ZTVN10__cxxabiv119__pointer_type_infoE, i64 2) to i8*), i8* getelementptr inbounds ([12 x i8], [12 x i8]* @_ZTSP9Exception, i32 0, i32 0), i32 0, i8* bitcast ({ i8*, i8*, i8* }* @_ZTI9Exception to i8*) }, align 8
@.str.1 = private unnamed_addr constant [45 x i8] c"Internal error: Unhandled exception detected\00", align 1
@.str.2 = private unnamed_addr constant [11 x i8] c"Error: %s\0A\00", align 1
@_ZTV9Exception = linkonce_odr unnamed_addr constant { [4 x i8*] } { [4 x i8*] [i8* null, i8* bitcast ({ i8*, i8*, i8* }* @_ZTI9Exception to i8*), i8* bitcast (void (%class.Exception*)* @_ZN9ExceptionD1Ev to i8*), i8* bitcast (void (%class.Exception*)* @_ZN9ExceptionD0Ev to i8*)] }, align 8
@_ZTV6Object = linkonce_odr unnamed_addr constant { [4 x i8*] } { [4 x i8*] [i8* null, i8* bitcast ({ i8*, i8* }* @_ZTI6Object to i8*), i8* bitcast (void (%class.Object*)* @_ZN6ObjectD1Ev to i8*), i8* bitcast (void (%class.Object*)* @_ZN6ObjectD0Ev to i8*)] }, align 8

; Function Attrs: noinline optnone ssp uwtable
define i32 @_Z3Barb(i1 zeroext %0) #0 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %2 = alloca i8, align 1
  %3 = alloca %class.Foo, align 4
  %4 = alloca i8*, align 8
  %5 = alloca i32, align 4
  %6 = zext i1 %0 to i8
  store i8 %6, i8* %2, align 1
  call void @_ZN3Foo9SetLengthEi(%class.Foo* %3, i32 17)
  %7 = load i8, i8* %2, align 1
  %8 = trunc i8 %7 to i1
  br i1 %8, label %9, label %25

9:                                                ; preds = %1
  %10 = call i8* @__cxa_allocate_exception(i64 8) #8
  %11 = bitcast i8* %10 to %class.Exception**
  %12 = invoke noalias nonnull i8* @_Znwm(i64 16) #9
          to label %13 unwind label %16

13:                                               ; preds = %9
  %14 = bitcast i8* %12 to %class.Exception*
  invoke void @_ZN9ExceptionC1EPKc(%class.Exception* %14, i8* getelementptr inbounds ([30 x i8], [30 x i8]* @.str, i64 0, i64 0))
          to label %15 unwind label %20

15:                                               ; preds = %13
  store %class.Exception* %14, %class.Exception** %11, align 16
  call void @__cxa_throw(i8* %10, i8* bitcast ({ i8*, i8*, i32, i8* }* @_ZTIP9Exception to i8*), i8* null) #10
  unreachable

16:                                               ; preds = %9
  %17 = landingpad { i8*, i32 }
          cleanup
  %18 = extractvalue { i8*, i32 } %17, 0
  store i8* %18, i8** %4, align 8
  %19 = extractvalue { i8*, i32 } %17, 1
  store i32 %19, i32* %5, align 4
  br label %24

20:                                               ; preds = %13
  %21 = landingpad { i8*, i32 }
          cleanup
  %22 = extractvalue { i8*, i32 } %21, 0
  store i8* %22, i8** %4, align 8
  %23 = extractvalue { i8*, i32 } %21, 1
  store i32 %23, i32* %5, align 4
  call void @_ZdlPv(i8* %12) #11
  br label %24

24:                                               ; preds = %20, %16
  call void @__cxa_free_exception(i8* %10) #8
  br label %27

25:                                               ; preds = %1
  call void @_ZN3Foo9SetLengthEi(%class.Foo* %3, i32 24)
  %26 = call i32 @_ZNK3Foo9GetLengthEv(%class.Foo* %3)
  ret i32 %26

27:                                               ; preds = %24
  %28 = load i8*, i8** %4, align 8
  %29 = load i32, i32* %5, align 4
  %30 = insertvalue { i8*, i32 } undef, i8* %28, 0
  %31 = insertvalue { i8*, i32 } %30, i32 %29, 1
  resume { i8*, i32 } %31
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN3Foo9SetLengthEi(%class.Foo* %0, i32 %1) #1 align 2 {
  %3 = alloca %class.Foo*, align 8
  %4 = alloca i32, align 4
  store %class.Foo* %0, %class.Foo** %3, align 8
  store i32 %1, i32* %4, align 4
  %5 = load %class.Foo*, %class.Foo** %3, align 8
  %6 = load i32, i32* %4, align 4
  %7 = getelementptr inbounds %class.Foo, %class.Foo* %5, i32 0, i32 0
  store i32 %6, i32* %7, align 4
  ret void
}

declare i8* @__cxa_allocate_exception(i64)

; Function Attrs: nobuiltin allocsize(0)
declare nonnull i8* @_Znwm(i64) #2

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
declare void @_ZdlPv(i8*) #3

declare void @__cxa_free_exception(i8*)

declare void @__cxa_throw(i8*, i8*, i8*)

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr i32 @_ZNK3Foo9GetLengthEv(%class.Foo* %0) #1 align 2 {
  %2 = alloca %class.Foo*, align 8
  store %class.Foo* %0, %class.Foo** %2, align 8
  %3 = load %class.Foo*, %class.Foo** %2, align 8
  %4 = getelementptr inbounds %class.Foo, %class.Foo* %3, i32 0, i32 0
  %5 = load i32, i32* %4, align 4
  ret i32 %5
}

; Function Attrs: noinline norecurse optnone ssp uwtable
define i32 @main(i32 %0, i8** %1) #4 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i8**, align 8
  %6 = alloca i32, align 4
  %7 = alloca i8, align 1
  %8 = alloca i32, align 4
  %9 = alloca i8*, align 8
  %10 = alloca i32, align 4
  %11 = alloca %class.Exception*, align 8
  store i32 0, i32* %3, align 4
  store i32 %0, i32* %4, align 4
  store i8** %1, i8*** %5, align 8
  %12 = load i32, i32* %4, align 4
  %13 = icmp sge i32 %12, 2
  %14 = zext i1 %13 to i8
  store i8 %14, i8* %7, align 1
  %15 = load i8, i8* %7, align 1
  %16 = trunc i8 %15 to i1
  %17 = invoke i32 @_Z3Barb(i1 zeroext %16)
          to label %18 unwind label %19

18:                                               ; preds = %2
  store i32 %17, i32* %8, align 4
  store i32 0, i32* %6, align 4
  br label %36

19:                                               ; preds = %2
  %20 = landingpad { i8*, i32 }
          catch i8* bitcast ({ i8*, i8*, i32, i8* }* @_ZTIP9Exception to i8*)
          catch i8* null
  %21 = extractvalue { i8*, i32 } %20, 0
  store i8* %21, i8** %9, align 8
  %22 = extractvalue { i8*, i32 } %20, 1
  store i32 %22, i32* %10, align 4
  br label %23

23:                                               ; preds = %19
  %24 = load i32, i32* %10, align 4
  %25 = call i32 @llvm.eh.typeid.for(i8* bitcast ({ i8*, i8*, i32, i8* }* @_ZTIP9Exception to i8*)) #8
  %26 = icmp eq i32 %24, %25
  br i1 %26, label %27, label %38

27:                                               ; preds = %23
  %28 = load i8*, i8** %9, align 8
  %29 = call i8* @__cxa_begin_catch(i8* %28) #8
  %30 = bitcast i8* %29 to %class.Exception*
  store %class.Exception* %30, %class.Exception** %11, align 8
  %31 = load %class.Exception*, %class.Exception** %11, align 8
  %32 = invoke i8* @_ZNK9Exception7GetTextEv(%class.Exception* %31)
          to label %33 unwind label %48

33:                                               ; preds = %27
  %34 = invoke i32 (i8*, ...) @printf(i8* getelementptr inbounds ([11 x i8], [11 x i8]* @.str.2, i64 0, i64 0), i8* %32)
          to label %35 unwind label %48

35:                                               ; preds = %33
  store i32 1, i32* %6, align 4
  call void @__cxa_end_catch() #8
  br label %36

36:                                               ; preds = %35, %42, %18
  %37 = load i32, i32* %6, align 4
  ret i32 %37

38:                                               ; preds = %23
  %39 = load i8*, i8** %9, align 8
  %40 = call i8* @__cxa_begin_catch(i8* %39) #8
  %41 = invoke i32 @puts(i8* getelementptr inbounds ([45 x i8], [45 x i8]* @.str.1, i64 0, i64 0))
          to label %42 unwind label %43

42:                                               ; preds = %38
  store i32 1, i32* %6, align 4
  call void @__cxa_end_catch()
  br label %36

43:                                               ; preds = %38
  %44 = landingpad { i8*, i32 }
          cleanup
  %45 = extractvalue { i8*, i32 } %44, 0
  store i8* %45, i8** %9, align 8
  %46 = extractvalue { i8*, i32 } %44, 1
  store i32 %46, i32* %10, align 4
  invoke void @__cxa_end_catch()
          to label %47 unwind label %57

47:                                               ; preds = %43
  br label %52

48:                                               ; preds = %33, %27
  %49 = landingpad { i8*, i32 }
          cleanup
  %50 = extractvalue { i8*, i32 } %49, 0
  store i8* %50, i8** %9, align 8
  %51 = extractvalue { i8*, i32 } %49, 1
  store i32 %51, i32* %10, align 4
  call void @__cxa_end_catch() #8
  br label %52

52:                                               ; preds = %48, %47
  %53 = load i8*, i8** %9, align 8
  %54 = load i32, i32* %10, align 4
  %55 = insertvalue { i8*, i32 } undef, i8* %53, 0
  %56 = insertvalue { i8*, i32 } %55, i32 %54, 1
  resume { i8*, i32 } %56

57:                                               ; preds = %43
  %58 = landingpad { i8*, i32 }
          catch i8* null
  %59 = extractvalue { i8*, i32 } %58, 0
  call void @__clang_call_terminate(i8* %59) #12
  unreachable
}

; Function Attrs: nounwind readnone
declare i32 @llvm.eh.typeid.for(i8*) #5

declare i8* @__cxa_begin_catch(i8*)

declare i32 @puts(i8*) #6

declare void @__cxa_end_catch()

; Function Attrs: noinline noreturn nounwind
define linkonce_odr hidden void @__clang_call_terminate(i8* %0) #7 {
  %2 = call i8* @__cxa_begin_catch(i8* %0) #8
  call void @_ZSt9terminatev() #12
  unreachable
}

declare void @_ZSt9terminatev()

declare i32 @printf(i8*, ...) #6

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr i8* @_ZNK9Exception7GetTextEv(%class.Exception* %0) #1 align 2 {
  %2 = alloca %class.Exception*, align 8
  store %class.Exception* %0, %class.Exception** %2, align 8
  %3 = load %class.Exception*, %class.Exception** %2, align 8
  %4 = getelementptr inbounds %class.Exception, %class.Exception* %3, i32 0, i32 1
  %5 = load i8*, i8** %4, align 8
  ret i8* %5
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN9ExceptionC2EPKc(%class.Exception* %0, i8* %1) unnamed_addr #1 align 2 {
  %3 = alloca %class.Exception*, align 8
  %4 = alloca i8*, align 8
  store %class.Exception* %0, %class.Exception** %3, align 8
  store i8* %1, i8** %4, align 8
  %5 = load %class.Exception*, %class.Exception** %3, align 8
  %6 = bitcast %class.Exception* %5 to %class.Object*
  call void @_ZN6ObjectC2Ev(%class.Object* %6) #8
  %7 = bitcast %class.Exception* %5 to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [4 x i8*] }, { [4 x i8*] }* @_ZTV9Exception, i32 0, inrange i32 0, i32 2) to i32 (...)**), i32 (...)*** %7, align 8
  %8 = getelementptr inbounds %class.Exception, %class.Exception* %5, i32 0, i32 1
  %9 = load i8*, i8** %4, align 8
  store i8* %9, i8** %8, align 8
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN6ObjectC2Ev(%class.Object* %0) unnamed_addr #1 align 2 {
  %2 = alloca %class.Object*, align 8
  store %class.Object* %0, %class.Object** %2, align 8
  %3 = load %class.Object*, %class.Object** %2, align 8
  %4 = bitcast %class.Object* %3 to i32 (...)***
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [4 x i8*] }, { [4 x i8*] }* @_ZTV6Object, i32 0, inrange i32 0, i32 2) to i32 (...)**), i32 (...)*** %4, align 8
  ret void
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr void @_ZN9ExceptionD1Ev(%class.Exception* %0) unnamed_addr #0 align 2 {
  %2 = alloca %class.Exception*, align 8
  store %class.Exception* %0, %class.Exception** %2, align 8
  %3 = load %class.Exception*, %class.Exception** %2, align 8
  call void @_ZN9ExceptionD2Ev(%class.Exception* %3)
  ret void
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr void @_ZN9ExceptionD0Ev(%class.Exception* %0) unnamed_addr #0 align 2 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %2 = alloca %class.Exception*, align 8
  %3 = alloca i8*, align 8
  %4 = alloca i32, align 4
  store %class.Exception* %0, %class.Exception** %2, align 8
  %5 = load %class.Exception*, %class.Exception** %2, align 8
  invoke void @_ZN9ExceptionD1Ev(%class.Exception* %5)
          to label %6 unwind label %8

6:                                                ; preds = %1
  %7 = bitcast %class.Exception* %5 to i8*
  call void @_ZdlPv(i8* %7) #11
  ret void

8:                                                ; preds = %1
  %9 = landingpad { i8*, i32 }
          cleanup
  %10 = extractvalue { i8*, i32 } %9, 0
  store i8* %10, i8** %3, align 8
  %11 = extractvalue { i8*, i32 } %9, 1
  store i32 %11, i32* %4, align 4
  %12 = bitcast %class.Exception* %5 to i8*
  call void @_ZdlPv(i8* %12) #11
  br label %13

13:                                               ; preds = %8
  %14 = load i8*, i8** %3, align 8
  %15 = load i32, i32* %4, align 4
  %16 = insertvalue { i8*, i32 } undef, i8* %14, 0
  %17 = insertvalue { i8*, i32 } %16, i32 %15, 1
  resume { i8*, i32 } %17
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr void @_ZN6ObjectD1Ev(%class.Object* %0) unnamed_addr #0 align 2 {
  %2 = alloca %class.Object*, align 8
  store %class.Object* %0, %class.Object** %2, align 8
  %3 = load %class.Object*, %class.Object** %2, align 8
  call void @_ZN6ObjectD2Ev(%class.Object* %3)
  ret void
}

; Function Attrs: noinline optnone ssp uwtable
define linkonce_odr void @_ZN6ObjectD0Ev(%class.Object* %0) unnamed_addr #0 align 2 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) {
  %2 = alloca %class.Object*, align 8
  %3 = alloca i8*, align 8
  %4 = alloca i32, align 4
  store %class.Object* %0, %class.Object** %2, align 8
  %5 = load %class.Object*, %class.Object** %2, align 8
  invoke void @_ZN6ObjectD1Ev(%class.Object* %5)
          to label %6 unwind label %8

6:                                                ; preds = %1
  %7 = bitcast %class.Object* %5 to i8*
  call void @_ZdlPv(i8* %7) #11
  ret void

8:                                                ; preds = %1
  %9 = landingpad { i8*, i32 }
          cleanup
  %10 = extractvalue { i8*, i32 } %9, 0
  store i8* %10, i8** %3, align 8
  %11 = extractvalue { i8*, i32 } %9, 1
  store i32 %11, i32* %4, align 4
  %12 = bitcast %class.Object* %5 to i8*
  call void @_ZdlPv(i8* %12) #11
  br label %13

13:                                               ; preds = %8
  %14 = load i8*, i8** %3, align 8
  %15 = load i32, i32* %4, align 4
  %16 = insertvalue { i8*, i32 } undef, i8* %14, 0
  %17 = insertvalue { i8*, i32 } %16, i32 %15, 1
  resume { i8*, i32 } %17
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN6ObjectD2Ev(%class.Object* %0) unnamed_addr #1 align 2 {
  %2 = alloca %class.Object*, align 8
  store %class.Object* %0, %class.Object** %2, align 8
  %3 = load %class.Object*, %class.Object** %2, align 8
  ret void
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define linkonce_odr void @_ZN9ExceptionD2Ev(%class.Exception* %0) unnamed_addr #1 align 2 {
  %2 = alloca %class.Exception*, align 8
  store %class.Exception* %0, %class.Exception** %2, align 8
  %3 = load %class.Exception*, %class.Exception** %2, align 8
  %4 = bitcast %class.Exception* %3 to %class.Object*
  call void @_ZN6ObjectD2Ev(%class.Object* %4)
  ret void
}

attributes #0 = { noinline optnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #1 = { noinline nounwind optnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #2 = { nobuiltin allocsize(0) "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #3 = { nobuiltin nounwind "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #4 = { noinline norecurse optnone ssp uwtable "darwin-stkchk-strong-link" "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #5 = { nounwind readnone }
attributes #6 = { "darwin-stkchk-strong-link" "frame-pointer"="all" "no-trapping-math"="true" "probe-stack"="___chkstk_darwin" "stack-protector-buffer-size"="8" "target-cpu"="penryn" "target-features"="+cx16,+cx8,+fxsr,+mmx,+sahf,+sse,+sse2,+sse3,+sse4.1,+ssse3,+x87" "tune-cpu"="generic" }
attributes #7 = { noinline noreturn nounwind }
attributes #8 = { nounwind }
attributes #9 = { builtin allocsize(0) }
attributes #10 = { noreturn }
attributes #11 = { builtin nounwind }
attributes #12 = { noreturn nounwind }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 2, !"SDK Version", [2 x i32] [i32 12, i32 3]}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{i32 7, !"PIC Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Apple clang version 13.1.6 (clang-1316.0.21.2.5)"}
