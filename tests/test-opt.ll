; ModuleID = 'tests/test-opt.bc'
source_filename = "tests/test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr global [26 x i8] c"5\09\08\12A\08\12A\00A\15\04\12\15\08\0F\06A\12\15\13\08\0F\06@a", align 1
@.str.1 = private unnamed_addr global [8 x i8] c"WWWWD\02ka", align 1
@.str.2 = private unnamed_addr global [7 x i8] c"65'^@ka", align 1

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main() #0 {
  %1 = call i8* @__decrypt(i8* getelementptr inbounds ([26 x i8], [26 x i8]* @.str, i64 0, i64 0))
  %2 = call i32 @puts(i8* getelementptr inbounds ([26 x i8], [26 x i8]* @.str, i64 0, i64 0))
  %3 = call i8* @__encrypt(i8* getelementptr inbounds ([26 x i8], [26 x i8]* @.str, i64 0, i64 0))
  %4 = call i32 @getchar()
  %5 = trunc i32 %4 to i8
  %6 = sext i8 %5 to i32
  %7 = icmp eq i32 %6, 54
  br i1 %7, label %8, label %13

8:                                                ; preds = %0
  %9 = sext i8 %5 to i32
  %10 = call i8* @__decrypt(i8* getelementptr inbounds ([8 x i8], [8 x i8]* @.str.1, i64 0, i64 0))
  %11 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([8 x i8], [8 x i8]* @.str.1, i64 0, i64 0), i32 %9)
  %12 = call i8* @__encrypt(i8* getelementptr inbounds ([8 x i8], [8 x i8]* @.str.1, i64 0, i64 0))
  br label %17

13:                                               ; preds = %0
  %14 = call i8* @__decrypt(i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.2, i64 0, i64 0))
  %15 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.2, i64 0, i64 0))
  %16 = call i8* @__encrypt(i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.2, i64 0, i64 0))
  br label %17

17:                                               ; preds = %13, %8
  ret i32 0
}

declare dso_local i32 @puts(i8*) #1

declare dso_local i32 @getchar() #1

declare dso_local i32 @printf(i8*, ...) #1

declare i8* @__decrypt(i8*)

declare i8* @__encrypt(i8*)

attributes #0 = { noinline nounwind uwtable "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 12.0.1"}
