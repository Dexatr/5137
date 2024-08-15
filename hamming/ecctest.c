#include <assert.h>
#include "ecclib.h"

void flip_bit(ecc_t *ecc, unsigned char *address, unsigned short bit_to_flip);

int main(void)
{
    ecc_t ECC;
    unsigned int offset=0; 
    int rc; 
    unsigned char byteToRead, byteToWrite;
    unsigned short bitToFlip;
    unsigned char *base_addr=enable_ecc_memory(&ECC);

    // To exhaustively test there are 8192 cases for 13-bit SECDED, 8 data, 5 parity
    //
    //    1 no error case
    //   13 SBE cases that can be detected and corrected
    //   78 DBE cases which is 13 choose 2 combinations that can only be detected
    // 8100 MBE cases that are triple, quadruple, etc. that cannot be reliably detected (FP, FN potential)
    //
    // Recall triple fault is very very unlikely - more than 2 SEUs in the same memory word before you read and correct it.

    // TEST CASE 0: Read after Write (No Error Case)
    printf("**** TEST CASE 0: Read after Write all ***********\n");
    write_byte(&ECC, base_addr + 0, (unsigned char)0xFF);

    // Read all of the locations back without injecting an error
    rc = read_byte(&ECC, base_addr + 0, &byteToRead);
    if (rc != NO_ERROR) {
        printf("Test Case 0 failed: Expected NO_ERROR, got %d\n", rc);
    } else {
        printf("Test Case 0 passed.\n");
    }
    printf("**** END TEST CASE 0 *****************************\n\n");

    traceOn();

    // SBE Cases (Single Bit Errors)
    for (bitToFlip = 0; bitToFlip < 13; bitToFlip++)
    {
        printf("**** TEST CASE (SBE) : Bit %hu ******\n", bitToFlip);
        write_byte(&ECC, base_addr + 0, (unsigned char)0xAB);
        flip_bit(&ECC, base_addr + 0, bitToFlip);

        rc = read_byte(&ECC, base_addr + offset, &byteToRead);
        if (rc != bitToFlip) {
            printf("Test Case (SBE) Bit %hu failed: Expected %hu, got %d\n", bitToFlip, bitToFlip, rc);
        } else {
            printf("Test Case (SBE) Bit %hu passed.\n", bitToFlip);
        }

        flip_bit(&ECC, base_addr + 0, bitToFlip);  // Correct the bit back
        rc = read_byte(&ECC, base_addr + offset, &byteToRead);
        if (rc != NO_ERROR) {
            printf("Test Case (SBE) Bit %hu failed after correction: Expected NO_ERROR, got %d\n", bitToFlip, rc);
        } else {
            printf("Test Case (SBE) Bit %hu passed after correction.\n", bitToFlip);
        }

        printf("**** END TEST CASE (SBE) : Bit %hu *****************************\n\n", bitToFlip);
    }

    // DBE Cases (Double Bit Errors)
    for (bitToFlip = 0; bitToFlip < 13; bitToFlip++)
    {
        for (unsigned short secondBitToFlip = bitToFlip + 1; secondBitToFlip < 13; secondBitToFlip++)
        {
            printf("**** TEST CASE (DBE) : Bit %hu and Bit %hu ******\n", bitToFlip, secondBitToFlip);
            write_byte(&ECC, base_addr + 0, (unsigned char)0xAB);
            flip_bit(&ECC, base_addr + 0, bitToFlip);
            flip_bit(&ECC, base_addr + 0, secondBitToFlip);

            rc = read_byte(&ECC, base_addr + offset, &byteToRead);
            if (rc != DOUBLE_BIT_ERROR) {
                printf("Test Case (DBE) Bit %hu and Bit %hu failed: Expected DOUBLE_BIT_ERROR, got %d\n", bitToFlip, secondBitToFlip, rc);
            } else {
                printf("Test Case (DBE) Bit %hu and Bit %hu passed.\n", bitToFlip, secondBitToFlip);
            }

            printf("**** END TEST CASE (DBE) : Bit %hu and Bit %hu *****************************\n\n", bitToFlip, secondBitToFlip);
        }
    }

    traceOff();

    // More POSITIVE testing - do read after write on all locations
    
    // TEST CASE 6: Read after Write all
    printf("**** TEST CASE 6: Read after Write all ***********\n");
    for(offset=0; offset < MEM_SIZE; offset++)
    {
        byteToWrite = (offset % 255);
        write_byte(&ECC, base_addr + offset, (unsigned char)byteToWrite);
    }

    // read all of the locations back without injecting an error
    for(offset=0; offset < MEM_SIZE; offset++) {
        rc = read_byte(&ECC, base_addr + offset, &byteToRead);
        if (rc != NO_ERROR) {
            printf("Test Case 6 failed at offset %u: Expected NO_ERROR, got %d\n", offset, rc);
        }
    }
    printf("**** END TEST CASE 6 *****************************\n\n");

    return NO_ERROR;
}

// flip bit in encoded word: pW p1 p2 d1 p3 d2 d3 d4 p4 d5 d6 d7 d8
// bit position:             00 01 02 03 04 05 06 07 08 09 10 11 12
void flip_bit(ecc_t *ecc, unsigned char *address, unsigned short bit_to_flip) {
    unsigned int offset = address - ecc->data_memory;
    unsigned char byte = 0;
    unsigned short data_bit_to_flip = 0, parity_bit_to_flip = 0;
    int data_flip = 1;

    switch(bit_to_flip)
    {
        // parity bit pW=0, p01 ... p04
        case 0: 
            parity_bit_to_flip = 0;
            data_flip = 0;
            break;
        case 1: 
            parity_bit_to_flip = 1;
            data_flip = 0;
            break;
        case 2:
            parity_bit_to_flip = 2;
            data_flip = 0;
            break;
        case 4:
            data_flip = 0;
            parity_bit_to_flip = 3;
            break;
        case 8:
            data_flip = 0;
            parity_bit_to_flip = 4;
            break;

        // data bit d01 ... d08
        case 3: 
            data_bit_to_flip = 1;
            break;
        case 5: 
            data_bit_to_flip = 2;
            break;
        case 6: 
            data_bit_to_flip = 3;
            break;
        case 7: 
            data_bit_to_flip = 4;
            break;
        case 9: 
            data_bit_to_flip = 5;
            break;
        case 10: 
            data_bit_to_flip = 6;
            break;
        case 11: 
            data_bit_to_flip = 7;
            break;
        case 12: 
            data_bit_to_flip = 8; 
            break;

        default:
            printf("flipped bit OUT OF RANGE\n");
            return;
    }

    if(data_flip)
    {
        printf("DATA  : request=%hu\n", bit_to_flip);
        printf("DATA  : bit to flip=%hu\n", data_bit_to_flip);

        byte = ecc->data_memory[offset];

        printf("DATA  : original byte    = 0x%02X\n", byte);
        byte ^= (1 << (8 - data_bit_to_flip));

        printf("DATA  : flipped bit byte = 0x%02X\n\n", byte);
        ecc->data_memory[offset] = byte;

    }
    else
    {
        printf("PARITY: request=%hu\n", bit_to_flip);
        printf("PARITY: bit to flip=%hu\n", parity_bit_to_flip);

        byte = ecc->code_memory[offset];

        printf("PARITY: original byte    = 0x%02X\n", byte);
        byte ^= (1 << (7 - parity_bit_to_flip));

        printf("PARITY: flipped bit byte = 0x%02X\n\n", byte);
        ecc->code_memory[offset] = byte;
    }
}
