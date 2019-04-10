#include <cstdio>
#include <cstdlib>

#include "fc.h"
#include "rnn.h"
#include "init.h"
#include "types.h"
#include "utils.h"
#include "config.h"
#include "softmax.h"
#include "constants.h"
#include "activation.h"

#define SAMPLE_NUM 1000
#define SAMPLE_LEN 50

#define abs(x) x > 0? x: 0

int main(int argc, char *argv[]) {
    // declare weights
    // embedding
    FDATA_T* word_embedding = (FDATA_T*) malloc(sizeof(FDATA_T) * WORD_NUM * WORD_SIZE);

    // RNN
    FDATA_T* rnn_last_state = (FDATA_T*) malloc(sizeof(FDATA_T) * RNN_BATCH_SIZE * RNN_STATE_SIZE);
    FDATA_T* rnn_input_state = (FDATA_T*) malloc(sizeof(FDATA_T) * RNN_BATCH_SIZE * RNN_INPUT_SIZE);
    FDATA_T* rnn_bias = (FDATA_T*) malloc(sizeof(FDATA_T) * RNN_STATE_SIZE);
    FDATA_T* rnn_kernel = (FDATA_T*) malloc(sizeof(FDATA_T) * RNN_INPUT_SIZE * RNN_STATE_SIZE);
    FDATA_T* rnn_recurrent_kernel = (FDATA_T*) malloc(sizeof(FDATA_T) * RNN_STATE_SIZE * RNN_STATE_SIZE);
    FDATA_T* rnn_output_state = (FDATA_T*) malloc(sizeof(FDATA_T) * RNN_BATCH_SIZE * RNN_STATE_SIZE);

    // FC
    //FDATA_T* fc_input_feature_map = malloc(sizeof(FDATA_T) * FC_BATCH_SIZE * FC_INPUT_SIZE);
    FDATA_T* fc_bias = (FDATA_T*) malloc(sizeof(FDATA_T) * FC_OUTPUT_SIZE);
    FDATA_T* fc_kernel = (FDATA_T*) malloc(sizeof(FDATA_T) * FC_INPUT_SIZE * FC_OUTPUT_SIZE);
    FDATA_T* fc_output_feature_map = (FDATA_T*) malloc(sizeof(FDATA_T) * FC_BATCH_SIZE * FC_OUTPUT_SIZE);
    FDATA_T* softmax_result = (FDATA_T*) malloc(sizeof(FDATA_T) * SM_BATCH_SIZE * SM_CLASS_SIZE);
    IDATA_T* argmax_result = (IDATA_T*) malloc(sizeof(IDATA_T) * SM_BATCH_SIZE);

    // load model in
    load_data<FDATA_T, LDATA_T>(EMBEDDINGS_FILE, word_embedding, WORD_NUM * WORD_SIZE);
    load_data<FDATA_T, LDATA_T>(SIMPLE_RNN_BIAS_FILE, rnn_bias, RNN_STATE_SIZE);
    load_data<FDATA_T, LDATA_T>(SIMPLE_RNN_KERNEL_FILE, rnn_kernel, RNN_INPUT_SIZE * RNN_STATE_SIZE);
    load_data<FDATA_T, LDATA_T>(SIMPLE_RNN_RECURRENT_KERNEL_FILE, rnn_recurrent_kernel, RNN_STATE_SIZE * RNN_STATE_SIZE);
    load_data<FDATA_T, LDATA_T>(DENSE_BIAS_FILE, fc_bias, FC_OUTPUT_SIZE);
    load_data<FDATA_T, LDATA_T>(DENSE_KERNEL_FILE, fc_kernel, FC_INPUT_SIZE * FC_OUTPUT_SIZE);

#ifdef DEBUG
    print_data<FDATA_T, LDATA_T>(fc_kernel, FC_INPUT_SIZE * FC_OUTPUT_SIZE);
#endif

    // load dataset in
    IDATA_T* sequences = (IDATA_T*) malloc(sizeof(IDATA_T) * SAMPLE_LEN * SAMPLE_NUM);
    IDATA_T* C_result = (IDATA_T*) malloc(sizeof(IDATA_T) * SAMPLE_NUM);
    IDATA_T* Keras_result = (IDATA_T*) malloc(sizeof(IDATA_T) * SAMPLE_NUM);
    IDATA_T* Actual_result = (IDATA_T*) malloc(sizeof(IDATA_T) * SAMPLE_NUM);
    load_data<IDATA_T, LDATA_T>("../../datasets/org_seq.txt", sequences, SAMPLE_LEN * SAMPLE_NUM);
    load_data<IDATA_T, LDATA_T>("../../datasets/rnn_result.txt", Keras_result, SAMPLE_NUM);
    load_data<IDATA_T, LDATA_T>("../../datasets/actual_result.txt", Actual_result, SAMPLE_NUM);

    // record result (correctness)
    LDATA_T count_Keras = 0;    // correct rate of Keras
    LDATA_T count_C = 0;        // correct rate of C
    LDATA_T count_times = SAMPLE_NUM / FC_BATCH_SIZE * RNN_BATCH_SIZE;

    // do inference and print the result
    for (LDATA_T compute_time = 0; compute_time < SAMPLE_NUM / FC_BATCH_SIZE; compute_time++) {
        // initialize last state to 0
        zero_init<FDATA_T, LDATA_T>(rnn_last_state, RNN_BATCH_SIZE * RNN_STATE_SIZE);
        for (LDATA_T i = 0; i < SAMPLE_LEN; i++) {
            for (LDATA_T j = 0; j < RNN_BATCH_SIZE; j++) {
                LDATA_T sample_index = compute_time * SAMPLE_LEN * RNN_BATCH_SIZE + j * SAMPLE_LEN + i;
                LDATA_T word_index = sequences[sample_index];
                LDATA_T word_embedding_index = word_index * WORD_SIZE;
                LDATA_T rnn_input_state_index = j * RNN_INPUT_SIZE;
                copy_data<FDATA_T, LDATA_T>(&word_embedding[word_embedding_index], &rnn_input_state[rnn_input_state_index], RNN_INPUT_SIZE);
            }
#ifdef DEBUG
            print_data<FDATA_T, LDATA_T>(rnn_input_state, RNN_INPUT_SIZE * RNN_BATCH_SIZE);
#endif
            rnn<FDATA_T>(rnn_last_state, rnn_input_state, rnn_bias, rnn_kernel, rnn_recurrent_kernel, rnn_output_state);
            act_tanh<FDATA_T, LDATA_T>(rnn_output_state, RNN_BATCH_SIZE * RNN_STATE_SIZE);
#ifdef DEBUG
            print_data<FDATA_T, LDATA_T>(rnn_output_state, RNN_STATE_SIZE * RNN_BATCH_SIZE);
#endif
            FDATA_T* temp = rnn_last_state;
            rnn_last_state = rnn_output_state;
            rnn_output_state = temp;
        }
        // no ReLu after FC layer, only softmax
#ifdef DEBUG
        print_data<FDATA_T, LDATA_T>(rnn_last_state, RNN_BATCH_SIZE * RNN_STATE_SIZE);
#endif
        fc<FDATA_T>(rnn_last_state, fc_bias, fc_kernel, fc_output_feature_map);
#ifdef DEBUG
        print_data<FDATA_T, LDATA_T>(fc_output_feature_map, FC_BATCH_SIZE * FC_OUTPUT_SIZE);
#endif
        softmax<FDATA_T>(fc_output_feature_map, softmax_result);
        argmax<FDATA_T, IDATA_T>(fc_output_feature_map, argmax_result);
        copy_data<IDATA_T, LDATA_T>(argmax_result, &C_result[compute_time * RNN_BATCH_SIZE], RNN_BATCH_SIZE);

        for (int i = compute_time * RNN_BATCH_SIZE; i < (compute_time + 1) * RNN_BATCH_SIZE; i++) {
            if (Keras_result[i] == Actual_result[i])
                count_Keras++;
            if (C_result[i] == Actual_result[i])
                count_C++;

#ifdef VERBOSE
            if (C_result[i] == Keras_result[i])
                printf("INFO: Sample %d:\t result: %d\n", i, C_result[i]);
            else {
                printf("INFO: Sample %d:\t C_result: %d\t Keras_result: %d\t Actual_result: %d\t P(%d): %f\t P(%d): %lf\t delta_P: %lf\n",
                        i, C_result[i], Keras_result[i], Actual_result[i],
                        C_result[i], softmax_result[(i - compute_time * RNN_BATCH_SIZE) * SM_CLASS_SIZE + C_result[i]],
                        Keras_result[i], softmax_result[(i - compute_time * RNN_BATCH_SIZE) * SM_CLASS_SIZE + Keras_result[i]],
                        abs(softmax_result[(i - compute_time * RNN_BATCH_SIZE) * SM_CLASS_SIZE + C_result[i]] -
                            softmax_result[(i - compute_time * RNN_BATCH_SIZE) * SM_CLASS_SIZE + Keras_result[i]]));
            }
#endif
        }
    }

    printf("INFO: Correctness:\n");
    printf("INFO:   Keras: %f\n", (float) count_Keras / count_times);
    printf("INFO:   C:     %f\n", (float) count_C / count_times);

    free(word_embedding);
    free(rnn_last_state);
    free(rnn_input_state);
    free(rnn_bias);
    free(rnn_kernel);
    free(rnn_recurrent_kernel);
    free(rnn_output_state);
    free(fc_bias);
    free(fc_kernel);
    free(fc_output_feature_map);
    free(softmax_result);
    free(argmax_result);
    free(sequences);
    free(C_result);
    free(Keras_result);
    free(Actual_result);

    return 0;
}