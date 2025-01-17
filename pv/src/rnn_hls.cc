#include "rnn.h"
#include "types.h"
#include "constants.h"
#include "utils.h"

#define TILE_BATCH 32

void load_input_state(FDATA_T input_state[RNN_BATCH_SIZE * RNN_INPUT_SIZE],
                      FDATA_T** input_state_reg,
                      LDATA_T start_batch);

void load_last_state(FDATA_T last_state[RNN_BATCH_SIZE * RNN_STATE_SIZE],
                     FDATA_T** last_state_reg,
                     LDATA_T start_batch);

void load_kernel(FDATA_T kernel[RNN_STATE_SIZE * RNN_INPUT_SIZE],
                 FDATA_T kernel_reg[RNN_INPUT_SIZE],
                 LDATA_T output_state_index);

void load_recurrent_kernel(
    FDATA_T recurrent_kernel[RNN_STATE_SIZE * RNN_STATE_SIZE],
    FDATA_T recurrent_kernel_reg[RNN_STATE_SIZE],
    LDATA_T output_state_index);

void compute(FDATA_T** input_state_reg,
             FDATA_T** last_state_reg,
             FDATA_T kernel_reg[RNN_INPUT_SIZE],
             FDATA_T recurrent_kernel_reg[RNN_STATE_SIZE],
             LDATA_T output_state_index,
             FDATA_T** output_state_reg);

void load_kernels_and_compute(
    FDATA_T** input_state_reg,
    FDATA_T** last_state_reg,
    FDATA_T kernel[RNN_STATE_SIZE * RNN_INPUT_SIZE],
    FDATA_T recurrent_kernel[RNN_STATE_SIZE * RNN_STATE_SIZE],
    FDATA_T** output_state_reg);


void save_output_state(FDATA_T** output_state_reg,
                       FDATA_T bias[RNN_STATE_SIZE],
                       FDATA_T output_state[RNN_BATCH_SIZE * RNN_STATE_SIZE],
                       LDATA_T start_batch_index);

void load_input_state(FDATA_T input_state[RNN_BATCH_SIZE * RNN_INPUT_SIZE],
                      FDATA_T** input_state_reg,
                      LDATA_T start_batch_index) {

  // load a batch of input state, the batch size is TILE_BATCH
  // start from start_batch_index
  // input_state  --- load to ---> input_state_reg

  for (LDATA_T batch_iter = 0; batch_iter < TILE_BATCH; batch_iter++) {

    LDATA_T input_state_start_index = (start_batch_index + batch_iter) *
                                      RNN_INPUT_SIZE;

    for (LDATA_T input_state_index = 0; input_state_index < RNN_INPUT_SIZE;
         input_state_index++) {
// // #pragma HLS UNROLL factor=2
// #pragma HLS PIPELINE

      input_state_reg[batch_iter][input_state_index] =
          input_state[input_state_start_index + input_state_index];
    }
  }
}

void load_last_state(FDATA_T last_state[RNN_BATCH_SIZE * RNN_STATE_SIZE],
                     FDATA_T** last_state_reg,
                     LDATA_T start_batch_index) {

  // load a batch of last state, the batch size is TILE_BATCH
  // start from start_batch_index
  // last_state  --- load to ---> last_state_reg

  for (LDATA_T batch_iter = 0; batch_iter < TILE_BATCH; batch_iter++) {

    LDATA_T last_state_start_index = (start_batch_index + batch_iter) *
                                    RNN_STATE_SIZE;
    for (LDATA_T last_state_index = 0; last_state_index < RNN_STATE_SIZE;
         last_state_index++) {
// // #pragma HLS UNROLL factor=2
// #pragma HLS PIPELINE

      last_state_reg[batch_iter][last_state_index] =
          last_state[last_state_start_index + last_state_index];
    }
  }
}


void load_kernel(FDATA_T kernel[RNN_STATE_SIZE * RNN_INPUT_SIZE],
                 FDATA_T kernel_reg[RNN_INPUT_SIZE],
                 LDATA_T output_state_index) {

  // load the (output_state_index)'th column of kernel
  // used this column do matrix multiplication
  // kernel --- load to ---> kernel_reg

  LDATA_T kernel_start_index = output_state_index * RNN_INPUT_SIZE;
  for (LDATA_T input_state_index = 0; input_state_index < RNN_INPUT_SIZE;
       input_state_index++) {
// #pragma HLS UNROLL factor=2
// #pragma HLS PIPELINE

    kernel_reg[input_state_index] =
        kernel[kernel_start_index + input_state_index];
  }
}

void load_recurrent_kernel(
    FDATA_T recurrent_kernel[RNN_STATE_SIZE * RNN_STATE_SIZE],
    FDATA_T recurrent_kernel_reg[RNN_STATE_SIZE],
    LDATA_T output_state_index) {

  // load the (output_state_index)'th column of recurrent_kernel
  // used this column do matrix multiplication
  // recurrent_kernel --- load to ---> recurrent_kernel_reg

  LDATA_T recurrent_kernel_start_index = output_state_index * RNN_STATE_SIZE;
  for (LDATA_T last_state_index = 0; last_state_index < RNN_STATE_SIZE;
       last_state_index++) {
// #pragma HLS UNROLL factor=2
// #pragma HLS PIPELINE

    recurrent_kernel_reg[last_state_index] =
        recurrent_kernel[recurrent_kernel_start_index + last_state_index];
  }
}

void compute(FDATA_T** input_state_reg,
             FDATA_T** last_state_reg,
             FDATA_T kernel_reg[RNN_INPUT_SIZE],
             FDATA_T recurrent_kernel_reg[RNN_STATE_SIZE],
             LDATA_T output_state_index,
             FDATA_T** output_state_reg) {

  // take a batch of input_state and last_state,
  //  compute the output state, and store into the output_state_reg
  // note that we don't add bias here, the bias addition will be done in
  //  function "save_output_state"
  // input: input_state_reg, last_state_reg, kernel_reg,
  //          recurrent_kernel_reg, output_state_index
  // output: output_state_reg

  FDATA_T** local_reg = malloc_2d_array(TILE_BATCH, RNN_STATE_SIZE + RNN_INPUT_SIZE);
// // #pragma HLS ARRAY_PARTITION variable=local_reg dim=2

  for (LDATA_T batch_iter = 0; batch_iter < TILE_BATCH; batch_iter++) {
// // #pragma HLS UNROLL complete factor=8

    for (LDATA_T input_state_index = 0; input_state_index < RNN_INPUT_SIZE;
         input_state_index++) {
// // #pragma HLS UNROLL complete factor=8

      local_reg[batch_iter][input_state_index] = 
          kernel_reg[input_state_index] *
          input_state_reg[batch_iter][input_state_index];
    }

    for (LDATA_T last_state_index = 0; last_state_index < RNN_STATE_SIZE;
         last_state_index++) {
// // #pragma HLS UNROLL complete factor=8

      local_reg[batch_iter][RNN_INPUT_SIZE + last_state_index] =
          recurrent_kernel_reg[last_state_index] *
          last_state_reg[batch_iter][last_state_index];
    }

    ////// HACKING, suppose RNN_STATE_SIZE + RNN_INPUT_SIZE = 228 /////

    // prefix sum
    for (LDATA_T i = 0; i < 114; i++) {
// // #pragma HLS UNROLL complete factor=8

      local_reg[batch_iter][i] = local_reg[batch_iter][i] +
                                 local_reg[batch_iter][114 + i];
    }

    for (LDATA_T i = 0; i < 57; i++) {
// // #pragma HLS UNROLL complete factor=8

      local_reg[batch_iter][i] = local_reg[batch_iter][i] + 
                                 local_reg[batch_iter][57 + i];
    }

    // 57 = 28 * 2 + 1 -> need 29 reg for next iteration
    // the 57'th number will be copy to 29'th reg
    for (LDATA_T i = 0; i < 28; i++) {
// // #pragma HLS UNROLL complete factor=8

      local_reg[batch_iter][i] = local_reg[batch_iter][i] +
                                 local_reg[batch_iter][28 + i];
    }
    local_reg[batch_iter][28] = local_reg[batch_iter][56];

    // 29 = 14 * 2 + 1 -> need 15 reg for next iteration
    // the 29'th number will be copy to 15'th reg
    for (LDATA_T i = 0; i < 14; i++) {
// // #pragma HLS UNROLL complete factor=8

      local_reg[batch_iter][i] = local_reg[batch_iter][i] +
                                 local_reg[batch_iter][14 + i];
    }
    local_reg[batch_iter][14] = local_reg[batch_iter][28];

    // 15 = 7 * 2 + 1 -> need 8 reg for next iteration
    // the 15'th number will be copy to 8'th reg
    for (LDATA_T i = 0; i < 7; i++) {
// // #pragma HLS UNROLL complete factor=8

      local_reg[batch_iter][i] = local_reg[batch_iter][i] +
                                 local_reg[batch_iter][7 + i];
    }
    local_reg[batch_iter][7] = local_reg[batch_iter][14];

    // from 8, regular prefix sum
    for (LDATA_T i = 0; i < 4; i++) {
// // #pragma HLS UNROLL complete factor=4

      local_reg[batch_iter][i] = local_reg[batch_iter][i] +
                                 local_reg[batch_iter][4 + i];
    }

    // from 8, regular prefix sum
    for (LDATA_T i = 0; i < 2; i++) {
// // #pragma HLS UNROLL complete factor=2

      local_reg[batch_iter][i] = local_reg[batch_iter][i] +
                                 local_reg[batch_iter][2 + i];
    }

    // from 8, regular prefix sum
    for (LDATA_T i = 0; i < 1; i++) {
// // #pragma HLS UNROLL complete factor=1

      local_reg[batch_iter][i] = local_reg[batch_iter][i] +
                                 local_reg[batch_iter][1 + i];
    }

    output_state_reg[batch_iter][output_state_index] =
        local_reg[batch_iter][0];
  }

  free_2d_array(local_reg, TILE_BATCH, RNN_STATE_SIZE + RNN_INPUT_SIZE);
}

void load_kernels_and_compute(
    FDATA_T** input_state_reg,
    FDATA_T** last_state_reg,
    FDATA_T kernel[RNN_STATE_SIZE * RNN_INPUT_SIZE],
    FDATA_T recurrent_kernel[RNN_STATE_SIZE * RNN_STATE_SIZE],
    FDATA_T** output_state_reg) {

  // take loaded states as input, with a batch size of TILE_BATCH
  // load kernel then compute
  // input: input_state_reg, last_state_reg, kernel_reg, recurrent_kernel_reg
  // output: output_state_reg
// #pragma HLS INLINE

  FDATA_T kernel_reg[RNN_INPUT_SIZE];
  FDATA_T recurrent_kernel_reg[RNN_STATE_SIZE];
// #pragma HLS ARRAY_PARTITION variable=kernel_reg dim=1
// #pragma HLS ARRAY_PARTITION variable=recurrent_kernel_reg dim=1

  for (LDATA_T output_state_index = 0; output_state_index < RNN_STATE_SIZE;
       output_state_index++) {
// #pragma HLS DATAFLOW

    // load
    load_kernel(kernel, kernel_reg, output_state_index);
    load_recurrent_kernel(recurrent_kernel, recurrent_kernel_reg,
                          output_state_index);

    // compute a batch of output (but haven't add bias)
    compute(input_state_reg, last_state_reg, kernel_reg,
            recurrent_kernel_reg, output_state_index, output_state_reg);
  }
}

void save_output_state(FDATA_T** output_state_reg,
                       FDATA_T bias[RNN_STATE_SIZE],
                       FDATA_T output_state[RNN_BATCH_SIZE * RNN_STATE_SIZE],
                       LDATA_T start_batch_index) {

  // the output state in register is not the final result,
  // add bias to finish computing and store them into BRAM
  // output_state_reg + bias --- load to ---> output_state

  for (LDATA_T batch_iter = 0; batch_iter < TILE_BATCH; batch_iter++) {

    LDATA_T output_state_start_index = (start_batch_index + batch_iter) * 
                                       RNN_STATE_SIZE;
    for (LDATA_T output_state_index = 0;
         output_state_index < RNN_STATE_SIZE; output_state_index++) {
// // #pragma HLS UNROLL factor=2
// #pragma HLS PIPELINE

      output_state[output_state_start_index + output_state_index] =
          bias[output_state_index] +
          output_state_reg[batch_iter][output_state_index];
    }
  }
}

template<>
void rnn(FDATA_T last_state[RNN_BATCH_SIZE * RNN_STATE_SIZE],
         FDATA_T input_state[RNN_BATCH_SIZE * RNN_INPUT_SIZE],
         FDATA_T bias[RNN_STATE_SIZE],
         FDATA_T kernel[RNN_STATE_SIZE * RNN_INPUT_SIZE],
         FDATA_T recurrent_kernel[RNN_STATE_SIZE * RNN_STATE_SIZE],
         FDATA_T output_state[RNN_BATCH_SIZE * RNN_STATE_SIZE]) {

  // please do INITIALIZATION before input output_state
  // ------- DIMENSION SETTING  ---------- *
  //
  //   input_state: RNN_BATCH_SIZE * RNN_INPUT_SIZE (None * 100)
  //   last_state: RNN_BATCH_SIZE * RNN_STATE_SIZE (None * 128)
  //   bias: RNN_STATE_SIZE (128)
  //   kernel: RNN_INPUT_SIZE * RNN_STATE_SIZE (100 * 128)
  //   recurrent_kernel: RNN_STATE_SIZE * RNN_STATE_SIZE (128 * 128)
  //   output_state: RNN_BATCH_SIZE * RNN_STATE_SIZE (None, 128)

  // declare registers and use array partition
  // tile = 32
  FDATA_T** input_state_reg = malloc_2d_array(TILE_BATCH, RNN_INPUT_SIZE);
  FDATA_T** last_state_reg = malloc_2d_array(TILE_BATCH, RNN_STATE_SIZE);
  FDATA_T** output_state_reg = malloc_2d_array(TILE_BATCH, RNN_STATE_SIZE);

// #pragma HLS ARRAY_PARTITION variable=input_state_reg dim=2
// #pragma HLS ARRAY_PARTITION variable=last_state_reg dim=2
// #pragma HLS ARRAY_PARTITION variable=output_state_reg dim=1

BATCH:
  for (LDATA_T batch_iter = 0; batch_iter < RNN_BATCH_SIZE / TILE_BATCH;
       batch_iter++) {
// #pragma HLS DATAFLOW

    // load
    load_input_state(input_state, input_state_reg, batch_iter * TILE_BATCH);
    load_last_state(last_state, last_state_reg, batch_iter * TILE_BATCH);

    // compute + load kernel
    load_kernels_and_compute(input_state_reg, last_state_reg, kernel,
                             recurrent_kernel, output_state_reg);

    // save
    save_output_state(output_state_reg, bias, output_state,
                      batch_iter * TILE_BATCH);
  }
  free_2d_array(input_state_reg, TILE_BATCH, RNN_INPUT_SIZE);
  free_2d_array(last_state_reg, TILE_BATCH, RNN_STATE_SIZE);
  free_2d_array(output_state_reg, TILE_BATCH, RNN_STATE_SIZE);
}

