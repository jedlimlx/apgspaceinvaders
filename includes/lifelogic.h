                // extract bits of neighbour count:
                "vpand %%ymm8, %%ymm11, %%ymm1 \n\t"
                "vpxor %%ymm11, %%ymm8, %%ymm8 \n\t"
                "vpxor %%ymm1, %%ymm9, %%ymm9 \n\t"
                // compute appropriate quaternary Boolean function:
                "vpxor %%ymm9, %%ymm8, %%ymm8 \n\t"
                "vpor %%ymm10, %%ymm12, %%ymm12 \n\t"
                "vpxor %%ymm9, %%ymm10, %%ymm10 \n\t"
                "vpand %%ymm12, %%ymm8, %%ymm8 \n\t"
                "vpand %%ymm8, %%ymm10, %%ymm10 \n\t"
