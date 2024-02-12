#include "gradient.h"
#include "utils.h"

// using namespace std;

void tiled_conv(
    fm_t fixp_layer1_ifmap[3][IM_SIZE][IM_SIZE],        // input image
    fm_t fixp_layer2_ifmap[32][IM_SIZE][IM_SIZE],       // output of conv1
    wt_t fixp_conv1_weights[32][3][3][3],
    wt_t fixp_conv1_bias[32]


)
{
    //--------------------------------------------------------------------------
    // Defines interface IO ports for HLS. 
    // You should NOT modify these pragmas.
    //--------------------------------------------------------------------------
    #pragma HLS INTERFACE m_axi depth=3*32*32   port=fixp_layer1_ifmap   bundle=fm
    #pragma HLS INTERFACE m_axi depth=32*32*32  port=fixp_layer2_ifmap   bundle=fm
    #pragma HLS INTERFACE m_axi depth=32*3*3*3    port=fixp_conv1_weights  bundle=wt
    #pragma HLS INTERFACE m_axi depth=32    port=fixp_conv1_bias  bundle=wt
    #pragma HLS INTERFACE s_axilite port=return


    std::cout <<"------------------------------\n";
    std::cout << "Entered tiled_conv function\n" ;

    //--------------------------------------------------------------------------
    // On-chip buffers
    // You should NOT modify the buffer dimensions!
    //--------------------------------------------------------------------------
    fm_t conv_in_buf[IN_BUF_DEPTH][IN_BUF_HEIGHT][IN_BUF_WIDTH];
    wt_t conv_wt_buf[OUT_BUF_DEPTH][IN_BUF_DEPTH][3][3];
    wt_t conv_bias_buf[OUT_BUF_DEPTH];
    fm_t conv_out_buf[OUT_BUF_DEPTH][OUT_BUF_HEIGHT][OUT_BUF_WIDTH];

    fm_t vecB_buf[BLOCK_SIZE_N];
    fm_t vecC_buf[BLOCK_SIZE_M];
    wt_t matA_buf[BLOCK_SIZE_M][BLOCK_SIZE_N];
    wt_t fc_bias_buf[BLOCK_SIZE_M];

    fm_t layer2_ifmap[32][IM_SIZE][IM_SIZE];
    // ------------------------ PRAGMAS ------------------------------ 

     #pragma HLS array_partition variable=conv_out_buf dim=1 complete
     #pragma HLS array_partition variable=conv_wt_buf  dim=1 complete

     #pragma HLS array_partition variable=conv_out_buf dim=2  complete
     #pragma HLS array_partition variable=conv_in_buf  dim=2  complete

    #pragma HLS array_partition variable=conv_out_buf dim=3  complete
    #pragma HLS array_partition variable=conv_in_buf  dim=3  complete

     #pragma HLS array_partition variable=conv_bias_buf  complete

    // #pragma HLS array_partition variable=vecC_buf complete
    // #pragma HLS array_partition variable=matA_buf dim=1 complete

    // ------------------------ LAYERS ------------------------------- 

    std::cout << "----------------FORWARD PASS---------------------\n";

    std::cout << "----------------Layer 1: Conv1---------------------\n";

    fp_conv1:
    for(int ti = 0; ti < IM_SIZE/OUT_BUF_HEIGHT; ti++)
    {
        for(int tj = 0; tj < IM_SIZE/OUT_BUF_WIDTH; tj++)
        {
            std::cout << "Processing Tile " << ti*IM_SIZE/OUT_BUF_WIDTH + tj + 1;
            std::cout << "/" << IM_SIZE/OUT_BUF_HEIGHT * IM_SIZE/OUT_BUF_WIDTH << std::endl;    

            for(int b=0; b<32/OUT_BUF_DEPTH; b++){

                for(int d=0; d<1; d++){

                    load_input_tile_block_from_DRAM <3,IM_SIZE,IM_SIZE>(
                        conv_in_buf,
                        fixp_layer1_ifmap,
                        ti,
                        tj,
                        d
                    );

                    load_conv_layer_params_from_DRAM <32,3> (
                        conv_wt_buf,
                        conv_bias_buf,
                        fixp_conv1_weights,
                        fixp_conv1_bias,
                        b,
                        d
                    );

                    conv_3x3(
                        conv_out_buf,
                        conv_in_buf,
                        conv_wt_buf,
                        conv_bias_buf,
                        d,
                        3,
                        32
                    );
                }

                store_output_tile_to_DRAM <32, IM_SIZE, IM_SIZE> (
                	fixp_layer2_ifmap,
                    conv_out_buf,
                    ti,
                    tj,
                    b,
                    0
                );
            }
        }
    }

}


void model_conv(
    fm_t input_feature_map[3][IM_SIZE][IM_SIZE],
    wt_t layer_weights[32][3][3][3],
    wt_t layer_bias[32],
    fm_t output_feature_map[32][32][32]
)
{
//--------------------------------------------------------------------------
// Performs convolution with padding along with adding bias
// No activation function at the end
//--------------------------------------------------------------------------

    fm_t current_ip;

    for(int oc=0; oc<32; oc++){
        for(int oh=0; oh<IM_SIZE; oh++){
            for(int ow=0; ow<IM_SIZE; ow++){
                for(int ic=0; ic<3; ic++){
                    for(int fh=0; fh<3; fh++){
                        for(int fw=0; fw<3; fw++){

                            // PADDING
                            if( (oh+fh-1) < 0 || \
                                (ow+fw-1) < 0 || \
                                (oh+fh) > 32 || \
                                (ow+fw) > 32){
                                    current_ip = 0;
                                }

                            else{
                                current_ip = input_feature_map[ic][oh+fh-1][ow+fw-1];
                            }

                            // MAC operation
                            if(ic == 0 && fh == 0 && fw == 0)
                                output_feature_map[oc][oh][ow]  =   current_ip * layer_weights[oc][ic][fh][fw] + layer_bias[oc];
                            else
                                output_feature_map[oc][oh][ow]  +=  current_ip * layer_weights[oc][ic][fh][fw];

                        }
                    }
                }
            }
        }
    }

}

