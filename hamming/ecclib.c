#include "ecclib.h"

// This code is based upon a simple Hamming SECDED code to simulate an ECC memory.
//
// The code has only been partially verified with cases noted in ecctest.c
//
// Verifying the remaining cases and debugging is left as an exercise for students and the formulation can be
// compared to - http://ecee.colorado.edu/~ecen5623/design/hamming-updated-examples.xlsx
//
// The trace feature will output bit patterns for the data, distributed parity and the overall parity that will match the 
// spreadsheet read left to right, most significant bit to least, but not that the indexing is increasing left to right.
//

static int printTrace=0;

void traceOn(void)
{
    printTrace=1;
}

void traceOff(void)
{
    printTrace=0;
}

int write_byte(ecc_t *ecc, unsigned char *address, unsigned char byteToWrite) {
    unsigned int offset = address - ecc->data_memory;
    unsigned char codeword=0;

    ecc->data_memory[offset]= byteToWrite;
    codeword = get_codeword(ecc, offset);
    ecc->code_memory[offset] = codeword;

    if(printTrace) { printf("WRITE : COMPUTED PARITY = ");print_code(codeword); }
    if(printTrace) { printf("WRITE : PARITY          = ");print_code_word(ecc, address); }
    if(printTrace) { printf("WRITE : DATA            = ");print_data_word(ecc, address); }
    if(printTrace) { printf("WRITE : ECODED          = ");print_encoded(ecc, address); }
    if(printTrace) { printf("\n"); }
    

    return NO_ERROR;
}


int read_byte(ecc_t *ecc, unsigned char *address, unsigned char *byteRead) {
    unsigned int offset = address - ecc->data_memory;
    unsigned char SYNDROME=0, pW2=0, pW=0, codeword=0;

    codeword = get_codeword(ecc, offset);


    // Computation is odd due to bit encoding
    //
    // Mask out all bits other than the syndrome bits
    //
    // Right shift 3 so p04 is least significant bit
    //
    // The bit order of the syndrome matches the spreadsheet, so we then have to reverse the order
    // of the 4 bits so C1 is the least significant bit
    //
    SYNDROME = (((codeword & SYNBITS) ^ ((ecc->code_memory[offset]) & SYNBITS)) >> 3);

    SYNDROME = ((SYNDROME & 0x08) >> 3) | ((SYNDROME & 0x04) >> 1) | ((SYNDROME & 0x02) << 1) | ((SYNDROME & 0x01) << 3);


    // look up the original overall parity based upon the memory data originally written
    //
    pW = ((ecc->code_memory[offset]) & PW_BIT);

    // recompute the second version of the overall parity based on current memory contents
    //
    pW2 |= (PW_BIT  &  ((
                              ((ecc->data_memory[offset] & DATA_BIT_8)>>0) ^
                              ((ecc->data_memory[offset] & DATA_BIT_7)>>1) ^
                              ((ecc->data_memory[offset] & DATA_BIT_6)>>2) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_5)>>3) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_4)>>4) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_3)>>5) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_2)>>6) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_1)>>7) ^
                              ((ecc->code_memory[offset] & P04_BIT)>>3) ^ 
                              ((ecc->code_memory[offset] & P03_BIT)>>4) ^ 
                              ((ecc->code_memory[offset] & P02_BIT)>>5) ^ 
                              ((ecc->code_memory[offset] & P01_BIT)>>6)
		             ) << 7 )
		); 


    if(printTrace) { printf("READ  : COMPUTED PARITY = ");print_code(codeword); }

    if(printTrace) { printf("READ  : PARITY          = ");print_code_word(ecc, address); }
    if(printTrace) { printf("READ  : DATA            = ");print_data_word(ecc, address); }
    if(printTrace) { printf("READ  : ENCODED         = ");print_encoded(ecc, address); }

    if(printTrace) { printf("READ  : SYNDROME        = %02u\n", SYNDROME); }

    if(printTrace) { printf("READ  : PW              = 0x%02X\n", pW); }
    if(printTrace) { printf("READ  : PW2             = 0x%02X\n", pW2); }
    if(printTrace) { printf("\n"); }
    
    // 1) if SYNDROME ==0 and pW == pW2, return NO_ERROR
    if((SYNDROME == 0) && (pW == pW2))
    {
        return NO_ERROR;
    }

    // 2) if SYNDROME ==0 and pW != pW2, return PW_ERROR
        if((SYNDROME == 0) && (pW != pW2))
    {
        // restore pW to PW2
        printf("PW ERROR\n\n");
        ecc->code_memory[offset] |= pW2 & PW_BIT;
        return PW_ERROR;
    }

    // 3) if SYNDROME !=0 and pW == pW2, return DOUBLE_BIT_ERROR
    if((SYNDROME != 0) && (pW == pW2))
    {
        printf("DOUBLE BIT ERROR\n\n");
        return DOUBLE_BIT_ERROR;
    }

    // 4) if SYNDROME !=0 and pW != pW2, SBE, return SYNDROME
    if((SYNDROME != 0) && (pW != pW2))
    {
        printf("SBE @ %d\n\n", SYNDROME);
        return SYNDROME;
    }

    // if we get here, something is seriously wrong like triple bit
    // or worse error, so return UNKNOWN_ERROR    
    return UNKNOWN_ERROR;
}


unsigned char *enable_ecc_memory(ecc_t *ecc){
    int idx;
    for(idx=0; idx < MEM_SIZE; idx++)
    {
	    ecc->code_memory[idx]=0;
	    ecc->data_memory[idx]=0;
    }

    return ecc->data_memory;
}


// this is made to read like the spreadsheet model
//
unsigned char get_codeword(ecc_t *ecc, unsigned int offset)
{

    unsigned char codeword=0;

    //printf("CODEWORD offset=%d\n", offset);

    // p01 - per spreadsheet model, compute even parity=0 over 7,5,4,2,1 bits
    codeword |= (P01_BIT  &  (
                              ((ecc->data_memory[offset] & DATA_BIT_7)>>1) ^
                              ((ecc->data_memory[offset] & DATA_BIT_5)>>3) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_4)>>4) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_2)>>6) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_1)>>7)
		             ) << 6
		); 


    // p02 - per spreadsheet modell, compute even parity=0 over 7,6,4,3,1 bits
    codeword |= (P02_BIT  &  (
                              ((ecc->data_memory[offset] & DATA_BIT_7)>>1) ^
                              ((ecc->data_memory[offset] & DATA_BIT_6)>>2) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_4)>>4) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_3)>>5) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_1)>>7)
		             ) << 5
		); 


    // p03 - per spreadsheet model, compute even parity=0 over 8,4,3,2 bits
    codeword |= (P03_BIT  &  (
                              ((ecc->data_memory[offset] & DATA_BIT_8)>>0) ^
                              ((ecc->data_memory[offset] & DATA_BIT_4)>>4) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_3)>>5) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_2)>>6)
		             ) << 4
		); 

    
    // p04 - per spreadsheet model, compute even parity=0 over 8,7,6,5 bits
    codeword |= (P04_BIT  &  (
                              ((ecc->data_memory[offset] & DATA_BIT_8)>>0) ^
                              ((ecc->data_memory[offset] & DATA_BIT_7)>>1) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_6)>>2) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_5)>>3)
		             ) << 3
		); 

    
    // pW - per spreadsheet model compute even parity=0 over all bits
    codeword |= (PW_BIT  &  ((
                              ((ecc->data_memory[offset] & DATA_BIT_8)>>0) ^
                              ((ecc->data_memory[offset] & DATA_BIT_7)>>1) ^
                              ((ecc->data_memory[offset] & DATA_BIT_6)>>2) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_5)>>3) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_4)>>4) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_3)>>5) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_2)>>6) ^ 
                              ((ecc->data_memory[offset] & DATA_BIT_1)>>7) ^
                              ((codeword & P04_BIT)>>3) ^ 
                              ((codeword & P03_BIT)>>4) ^ 
                              ((codeword & P02_BIT)>>5) ^ 
                              ((codeword & P01_BIT)>>6)
		             ) << 7)
		); 

    return codeword;
}


void print_code(unsigned char codeword)
{
    printf("codeword=");
    if(codeword & PW_BIT) printf("1"); else printf("0");
    if(codeword & P01_BIT) printf("1"); else printf("0");
    if(codeword & P02_BIT) printf("1"); else printf("0");
    if(codeword & P03_BIT) printf("1"); else printf("0");
    if(codeword & P04_BIT) printf("1"); else printf("0");
    printf(" [0x%02X]", codeword);

    printf("\n");
}


void print_code_word(ecc_t *ecc, unsigned char *address) {
    unsigned int offset = address - ecc->data_memory;
    unsigned char codeword = ecc->code_memory[offset];

    printf("addr=%p (offset=%d) ", address, offset);
    if(codeword & PW_BIT) printf("1"); else printf("0");
    if(codeword & P01_BIT) printf("1"); else printf("0");
    if(codeword & P02_BIT) printf("1"); else printf("0");
    printf("_");
    if(codeword & P03_BIT) printf("1"); else printf("0");
    printf("___");
    if(codeword & P04_BIT) printf("1"); else printf("0");
    printf("____");
    printf("\n");
}


void print_data_word(ecc_t *ecc, unsigned char *address)
{
    unsigned int offset = address - ecc->data_memory;
    unsigned char dataword = ecc->data_memory[offset];

    printf("addr=%p (offset=%d) ", address, offset);

    printf("___");
    if(dataword & DATA_BIT_1) printf("1"); else printf("0");
    printf("_");
    if(dataword & DATA_BIT_2) printf("1"); else printf("0");
    if(dataword & DATA_BIT_3) printf("1"); else printf("0");
    if(dataword & DATA_BIT_4) printf("1"); else printf("0");
    printf("_");
    if(dataword & DATA_BIT_5) printf("1"); else printf("0");
    if(dataword & DATA_BIT_6) printf("1"); else printf("0");
    if(dataword & DATA_BIT_7) printf("1"); else printf("0");
    if(dataword & DATA_BIT_8) printf("1"); else printf("0");

    printf("    => ");
    if(dataword & DATA_BIT_1) printf("1"); else printf("0");
    if(dataword & DATA_BIT_2) printf("1"); else printf("0");
    if(dataword & DATA_BIT_3) printf("1"); else printf("0");
    if(dataword & DATA_BIT_4) printf("1"); else printf("0");
    printf(" ");
    if(dataword & DATA_BIT_5) printf("1"); else printf("0");
    if(dataword & DATA_BIT_6) printf("1"); else printf("0");
    if(dataword & DATA_BIT_7) printf("1"); else printf("0");
    if(dataword & DATA_BIT_8) printf("1"); else printf("0");
    printf(" [0x%02X]", dataword);

    printf("\n");
}


void print_encoded(ecc_t *ecc, unsigned char *address)
{
    unsigned int offset = address - ecc->data_memory;
    unsigned char codeword = ecc->code_memory[offset];
    unsigned char dataword = ecc->data_memory[offset];

    printf("addr=%p (offset=%d) ", address, offset);

    if(codeword & PW_BIT) printf("1"); else printf("0");
    if(codeword & P01_BIT) printf("1"); else printf("0");
    if(codeword & P02_BIT) printf("1"); else printf("0");
    if(dataword & DATA_BIT_1) printf("1"); else printf("0");
    if(codeword & P03_BIT) printf("1"); else printf("0");
    if(dataword & DATA_BIT_2) printf("1"); else printf("0");
    if(dataword & DATA_BIT_3) printf("1"); else printf("0");
    if(dataword & DATA_BIT_4) printf("1"); else printf("0");
    if(codeword & P04_BIT) printf("1"); else printf("0");
    if(dataword & DATA_BIT_5) printf("1"); else printf("0");
    if(dataword & DATA_BIT_6) printf("1"); else printf("0");
    if(dataword & DATA_BIT_7) printf("1"); else printf("0");
    if(dataword & DATA_BIT_8) printf("1"); else printf("0");

    printf("\n");
}

