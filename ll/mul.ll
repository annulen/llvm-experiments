define i32 @multiply(i32 %X) {
    %result = mul i32 %X, 8
    ret i32 %result
}

; Strenght reduction
define i32 @multiply_v2(i32 %X) {
    %result = shl i32 %X, 3
    ret i32 %result
}

; With temporaries
define i32 @multiply_v3(i32 %X) {
    %1 = add i32 %X, %X ; t1 = x + x
    %2 = add i32 %1, %1 ; t2 = t1 + t1
    %3 = add i32 %2, %2 ; t3 = t2 + t2
    ret i32 %2
}
