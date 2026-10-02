#include "PPintrin.h"
// implementation of absSerial(), but it is vectorized using PP intrinsics
void absVector(float *values, float *output, int N)
{
  __pp_vec_float x;
  __pp_vec_float result;
  __pp_vec_float zero = _pp_vset_float(0.f);
  __pp_mask maskAll, maskIsNegative, maskIsNotNegative;

  //  Note: Take a careful look at this loop indexing.  This example
  //  code is not guaranteed to work when (N % VECTOR_WIDTH) != 0.
  //  Why is that the case?
  for (int i = 0; i < N; i += VECTOR_WIDTH)
  {

    // All ones
    maskAll = _pp_init_ones();

    // All zeros
    maskIsNegative = _pp_init_ones(0);

    // Load vector of values from contiguous memory addresses
    _pp_vload_float(x, values + i, maskAll); // x = values[i];

    // Set mask according to predicate
    _pp_vlt_float(maskIsNegative, x, zero, maskAll); // if (x < 0) {

    // Execute instruction using mask ("if" clause)
    _pp_vsub_float(result, zero, x, maskIsNegative); //   output[i] = -x;

    // Inverse maskIsNegative to generate "else" mask
    maskIsNotNegative = _pp_mask_not(maskIsNegative); // } else {

    // Execute instruction ("else" clause)
    _pp_vload_float(result, values + i, maskIsNotNegative); //   output[i] = x; }

    // Write results back to memory
    _pp_vstore_float(output + i, result, maskAll);
  }
}

void clampedExpVector(float *values, int *exponents, float *output, int N)
{
  //
  // PP STUDENTS TODO: Implement your vectorized version of
  // clampedExpSerial() here.
  //
  // Your solution should work for any value of
  // N and VECTOR_WIDTH, not just when VECTOR_WIDTH divides N
  //
  int trueLength = N;
  if(N%VECTOR_WIDTH != 0){
    trueLength = N + (VECTOR_WIDTH - (N % VECTOR_WIDTH));
  }
  __pp_vec_float x;
  __pp_vec_int exp;
  __pp_vec_int allZero = _pp_vset_int(0);
  __pp_vec_int allOne = _pp_vset_int(1);
  __pp_vec_float clamp = _pp_vset_float(9.999999f);
  __pp_mask  maskAll, maskIsNotZero, maskGT9;
  for (int i = 0; i < trueLength; i += VECTOR_WIDTH)
  {
    int offset = (i + VECTOR_WIDTH > N) ? (N - i) : VECTOR_WIDTH;
    maskAll = _pp_init_ones(offset);
    maskIsNotZero = _pp_init_ones(0);
    maskGT9 = _pp_init_ones(0);
    _pp_vload_float(x, values + i, maskAll);
    _pp_vload_int(exp, exponents + i, maskAll);
    __pp_vec_float result = _pp_vset_float(1.f);

    // 1. set result to 1.0 if exponent is 0
    _pp_vgt_int(maskIsNotZero, exp, allZero, maskAll);
    // 2. exist exp not == 0, still need to compute the result.
    while(_pp_cntbits(maskIsNotZero) > 0){
      _pp_vmult_float(result, result, x, maskIsNotZero);
      _pp_vsub_int(exp, exp, allOne, maskAll);
      _pp_vgt_int(maskIsNotZero, exp, allZero, maskAll);
    }
    // 3. clamp > 9.999
    _pp_vgt_float(maskGT9, result, clamp, maskAll);
    _pp_vset_float(result, 9.999999f, maskGT9);
    _pp_vstore_float(output + i, result, maskAll);
  }
}

// returns the sum of all elements in values
// You can assume N is a multiple of VECTOR_WIDTH
// You can assume VECTOR_WIDTH is a power of 2
float arraySumVector(float *values, int N)
{

  //
  // PP STUDENTS TODO: Implement your vectorized version of arraySumSerial here
  //

  for (int i = 0; i < N; i += VECTOR_WIDTH)
  {
  }

  return 0.0;
}