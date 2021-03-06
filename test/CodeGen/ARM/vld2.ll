; RUN: llc < %s -march=arm -mattr=+neon | FileCheck %s

%struct.__neon_int8x8x2_t = type { <8 x i8>,  <8 x i8> }
%struct.__neon_int16x4x2_t = type { <4 x i16>, <4 x i16> }
%struct.__neon_int32x2x2_t = type { <2 x i32>, <2 x i32> }
%struct.__neon_float32x2x2_t = type { <2 x float>, <2 x float> }
%struct.__neon_int64x1x2_t = type { <1 x i64>, <1 x i64> }

%struct.__neon_int8x16x2_t = type { <16 x i8>,  <16 x i8> }
%struct.__neon_int16x8x2_t = type { <8 x i16>, <8 x i16> }
%struct.__neon_int32x4x2_t = type { <4 x i32>, <4 x i32> }
%struct.__neon_float32x4x2_t = type { <4 x float>, <4 x float> }

define <8 x i8> @vld2i8(i8* %A) nounwind {
;CHECK: vld2i8:
;CHECK: vld2.8
	%tmp1 = call %struct.__neon_int8x8x2_t @llvm.arm.neon.vld2.v8i8(i8* %A)
        %tmp2 = extractvalue %struct.__neon_int8x8x2_t %tmp1, 0
        %tmp3 = extractvalue %struct.__neon_int8x8x2_t %tmp1, 1
        %tmp4 = add <8 x i8> %tmp2, %tmp3
	ret <8 x i8> %tmp4
}

define <4 x i16> @vld2i16(i16* %A) nounwind {
;CHECK: vld2i16:
;CHECK: vld2.16
	%tmp0 = bitcast i16* %A to i8*
	%tmp1 = call %struct.__neon_int16x4x2_t @llvm.arm.neon.vld2.v4i16(i8* %tmp0)
        %tmp2 = extractvalue %struct.__neon_int16x4x2_t %tmp1, 0
        %tmp3 = extractvalue %struct.__neon_int16x4x2_t %tmp1, 1
        %tmp4 = add <4 x i16> %tmp2, %tmp3
	ret <4 x i16> %tmp4
}

define <2 x i32> @vld2i32(i32* %A) nounwind {
;CHECK: vld2i32:
;CHECK: vld2.32
	%tmp0 = bitcast i32* %A to i8*
	%tmp1 = call %struct.__neon_int32x2x2_t @llvm.arm.neon.vld2.v2i32(i8* %tmp0)
        %tmp2 = extractvalue %struct.__neon_int32x2x2_t %tmp1, 0
        %tmp3 = extractvalue %struct.__neon_int32x2x2_t %tmp1, 1
        %tmp4 = add <2 x i32> %tmp2, %tmp3
	ret <2 x i32> %tmp4
}

define <2 x float> @vld2f(float* %A) nounwind {
;CHECK: vld2f:
;CHECK: vld2.32
	%tmp0 = bitcast float* %A to i8*
	%tmp1 = call %struct.__neon_float32x2x2_t @llvm.arm.neon.vld2.v2f32(i8* %tmp0)
        %tmp2 = extractvalue %struct.__neon_float32x2x2_t %tmp1, 0
        %tmp3 = extractvalue %struct.__neon_float32x2x2_t %tmp1, 1
        %tmp4 = fadd <2 x float> %tmp2, %tmp3
	ret <2 x float> %tmp4
}

define <1 x i64> @vld2i64(i64* %A) nounwind {
;CHECK: vld2i64:
;CHECK: vld1.64
	%tmp0 = bitcast i64* %A to i8*
	%tmp1 = call %struct.__neon_int64x1x2_t @llvm.arm.neon.vld2.v1i64(i8* %tmp0)
        %tmp2 = extractvalue %struct.__neon_int64x1x2_t %tmp1, 0
        %tmp3 = extractvalue %struct.__neon_int64x1x2_t %tmp1, 1
        %tmp4 = add <1 x i64> %tmp2, %tmp3
	ret <1 x i64> %tmp4
}

define <16 x i8> @vld2Qi8(i8* %A) nounwind {
;CHECK: vld2Qi8:
;CHECK: vld2.8
	%tmp1 = call %struct.__neon_int8x16x2_t @llvm.arm.neon.vld2.v16i8(i8* %A)
        %tmp2 = extractvalue %struct.__neon_int8x16x2_t %tmp1, 0
        %tmp3 = extractvalue %struct.__neon_int8x16x2_t %tmp1, 1
        %tmp4 = add <16 x i8> %tmp2, %tmp3
	ret <16 x i8> %tmp4
}

define <8 x i16> @vld2Qi16(i16* %A) nounwind {
;CHECK: vld2Qi16:
;CHECK: vld2.16
	%tmp0 = bitcast i16* %A to i8*
	%tmp1 = call %struct.__neon_int16x8x2_t @llvm.arm.neon.vld2.v8i16(i8* %tmp0)
        %tmp2 = extractvalue %struct.__neon_int16x8x2_t %tmp1, 0
        %tmp3 = extractvalue %struct.__neon_int16x8x2_t %tmp1, 1
        %tmp4 = add <8 x i16> %tmp2, %tmp3
	ret <8 x i16> %tmp4
}

define <4 x i32> @vld2Qi32(i32* %A) nounwind {
;CHECK: vld2Qi32:
;CHECK: vld2.32
	%tmp0 = bitcast i32* %A to i8*
	%tmp1 = call %struct.__neon_int32x4x2_t @llvm.arm.neon.vld2.v4i32(i8* %tmp0)
        %tmp2 = extractvalue %struct.__neon_int32x4x2_t %tmp1, 0
        %tmp3 = extractvalue %struct.__neon_int32x4x2_t %tmp1, 1
        %tmp4 = add <4 x i32> %tmp2, %tmp3
	ret <4 x i32> %tmp4
}

define <4 x float> @vld2Qf(float* %A) nounwind {
;CHECK: vld2Qf:
;CHECK: vld2.32
	%tmp0 = bitcast float* %A to i8*
	%tmp1 = call %struct.__neon_float32x4x2_t @llvm.arm.neon.vld2.v4f32(i8* %tmp0)
        %tmp2 = extractvalue %struct.__neon_float32x4x2_t %tmp1, 0
        %tmp3 = extractvalue %struct.__neon_float32x4x2_t %tmp1, 1
        %tmp4 = fadd <4 x float> %tmp2, %tmp3
	ret <4 x float> %tmp4
}

declare %struct.__neon_int8x8x2_t @llvm.arm.neon.vld2.v8i8(i8*) nounwind readonly
declare %struct.__neon_int16x4x2_t @llvm.arm.neon.vld2.v4i16(i8*) nounwind readonly
declare %struct.__neon_int32x2x2_t @llvm.arm.neon.vld2.v2i32(i8*) nounwind readonly
declare %struct.__neon_float32x2x2_t @llvm.arm.neon.vld2.v2f32(i8*) nounwind readonly
declare %struct.__neon_int64x1x2_t @llvm.arm.neon.vld2.v1i64(i8*) nounwind readonly

declare %struct.__neon_int8x16x2_t @llvm.arm.neon.vld2.v16i8(i8*) nounwind readonly
declare %struct.__neon_int16x8x2_t @llvm.arm.neon.vld2.v8i16(i8*) nounwind readonly
declare %struct.__neon_int32x4x2_t @llvm.arm.neon.vld2.v4i32(i8*) nounwind readonly
declare %struct.__neon_float32x4x2_t @llvm.arm.neon.vld2.v4f32(i8*) nounwind readonly
