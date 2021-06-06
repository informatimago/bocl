#include <stdio.h>

#include "kernel_types.h"


#define S(x) #x

int main(void){
    printf("%-30s %-30s %"WORD_FORMAT"\n",S(WORD_MIN),S(INT64_MIN),INT64_MIN);
    printf("%-30s %-30s %"WORD_FORMAT"\n",S(WORD_MAX),S(INT64_MAX),INT64_MAX);
    printf("%-30s %-30s %"UWORD_FORMAT"\n",S(UWORD_MAX),S(UINT64_MAX),UINT64_MAX);
    printf("%-30s %-30s %"HALFWORD_FORMAT"\n",S(HALFWORD_MAX),S(UINT32_MAX),UINT32_MAX);

    printf("%-30s %s\n",S(WORD_FORMAT),WORD_FORMAT);
    printf("%-30s %s\n",S(UWORD_FORMAT),UWORD_FORMAT);
    printf("%-30s %s\n",S(HALFWORD_FORMAT),HALFWORD_FORMAT);
    printf("%-30s %s\n",S(WORD_X_FORMAT),WORD_X_FORMAT);
    printf("%-30s %s\n",S(UWORD_X_FORMAT),UWORD_X_FORMAT);
    printf("%-30s %s\n",S(HALFWORD_X_FORMAT),HALFWORD_X_FORMAT);

    return 0;}
