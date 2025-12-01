#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/time.h>

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef unsigned int LONG;

struct tagBITMAPFILEHEADER
{
    WORD bfType; //specifies the file type
    DWORD bfSize; //specifies the size in bytes of the bitmap file
    WORD bfReserved1; //reserved; must be 0
    WORD bfReserved2; //reserved; must be 0
    DWORD bfOffBits; //species the offset in bytes from the bitmapfileheader to the bitmap bits
};

struct tagBITMAPINFOHEADER
{
    DWORD biSize; //specifies the number of bytes required by the struct
    LONG biWidth; //specifies width in pixels
    LONG biHeight; //specifies height in pixels
    WORD biPlanes; //specifies the number of color planes, must be 1;
    WORD biBitCount; //specifies the number of bit per pixel
    DWORD biCompression; //specifies the type of compression
    DWORD biSizeImage; //size of image in bytes
    LONG biXPelsPerMeter; //number of pixels per meter in x axis
    LONG biYPelsPerMeter; //number of pixels per meter in y axis
    DWORD biClrUsed; //number of colors used by the bitmap
    DWORD biClrImportant; //number of colors that are important
};

struct HuffmanEntry {
    BYTE value;      // The color value
    BYTE bitLength;  // Number of bits in the code
    DWORD code;      // The Huffman code packed into integer
};

struct HuffmanTable
{
    int numEntries;               // # of entries
    HuffmanEntry entries[256];    // Max 256 values
};

class hte{
    public:
        int val;
        hte *l, *r;
        hte(){
            l = r = NULL;
        }
};

hte* build_huffman_tree(HuffmanTable &table){
    hte* root = new hte();

    for (int i = 0; i < table.numEntries; i++){
        DWORD code = table.entries[i].code;
        int bits = table.entries[i].bitLength;
        BYTE val = table.entries[i].value;

        hte* node = root;

        for(int j = bits - 1; j >= 0; j--){
            int bit = (code>>j) & 1;

            if(bit == 0){
                if(!node->l) node->l = new hte();
                node = node->l;
            } else {
                if(!node->r) node->r = new hte();
                node = node->r;
            }
        }
        node->val = val;
    }
    return root;
}

void decode_channel(BYTE *decoded, int pixelCount, BYTE* encoded_data, int bit_count, hte* root, int divisor){
    int bitp = 0;
    int pixelIndex = 0;
    hte* node = root;

    int totalBits = bit_count * 8;

    for(int i = 0; i < totalBits && pixelIndex < pixelCount; i++){
        int byteIndex = bitp / 8;
        int bitIndex = 7 - (bitp % 8);

        int bit = (encoded_data[byteIndex] >> bitIndex) & 1;
        bitp++;

        node = (bit == 0) ? node->l : node->r;

        if (!node->l && !node->r) {
            BYTE val = node->val;
            if(val > 255) val = 255;
            decoded[pixelIndex++] = val;
            node = root;
        }
    }
}

int main(int argc, char *argv[]) {
    
    if(argc != 3){
        printf("Incorrect # of inputs");
        return 0;
    }
    
    char *input = argv[1];
    char *output = argv[2];

    FILE *inf = fopen(input, "rb");

    struct tagBITMAPFILEHEADER fileHeader;
    struct tagBITMAPINFOHEADER infoHeader;
    
    fread(&fileHeader.bfType, 2, 1, inf);
    fread(&fileHeader.bfSize, 4, 1, inf);
    fread(&fileHeader.bfReserved1, 2, 1, inf);
    fread(&fileHeader.bfReserved2, 2, 1, inf);
    fread(&fileHeader.bfOffBits, 4, 1, inf);
    fread(&infoHeader, sizeof(infoHeader), 1, inf);

    int bit_countR, bit_countG, bit_countB;
    fread(&bit_countR, sizeof(int), 1, inf);
    fread(&bit_countG, sizeof(int), 1, inf);
    fread(&bit_countB, sizeof(int), 1, inf);

    int encoded_bytesR = (bit_countR + 7)/8;
    int encoded_bytesG = (bit_countG + 7)/8;
    int encoded_bytesB = (bit_countB + 7)/8;

    BYTE* encodedR = new BYTE[encoded_bytesR];
    BYTE* encodedG = new BYTE[encoded_bytesG];
    BYTE* encodedB = new BYTE[encoded_bytesB];

    fread(encodedR, 1, encoded_bytesR, inf);
    fread(encodedG, 1, encoded_bytesG, inf);
    fread(encodedB, 1, encoded_bytesB, inf);

    HuffmanTable tableR, tableG, tableB;
    fread(&tableR.numEntries, sizeof(int), 1, inf);
    fread(tableR.entries, sizeof(HuffmanEntry), tableR.numEntries, inf);
    fread(&tableG.numEntries, sizeof(int), 1, inf);
    fread(tableG.entries, sizeof(HuffmanEntry), tableG.numEntries, inf);
    fread(&tableB.numEntries, sizeof(int), 1, inf);
    fread(tableB.entries, sizeof(HuffmanEntry), tableB.numEntries, inf);

    int divisor;
    fread(&divisor, sizeof(int), 1, inf);

    fclose(inf);

    int pixelCount = infoHeader.biWidth * infoHeader.biHeight;

    BYTE* decodedR = new BYTE[pixelCount];
    BYTE* decodedG = new BYTE[pixelCount];
    BYTE* decodedB = new BYTE[pixelCount];

    hte* rootR = build_huffman_tree(tableR);
    hte* rootG = build_huffman_tree(tableG);
    hte* rootB = build_huffman_tree(tableB);

    decode_channel(decodedR, pixelCount, encodedR, bit_countR, rootR, divisor);
    decode_channel(decodedG, pixelCount, encodedG, bit_countG, rootG, divisor);
    decode_channel(decodedB, pixelCount, encodedB, bit_countB, rootB, divisor);

    int padding = (4 - (infoHeader.biWidth * 3) % 4) % 4;
    int rowSize = infoHeader.biWidth * 3 + padding;
    BYTE* data = new BYTE[rowSize * infoHeader.biHeight];

    for (int y = 0; y < infoHeader.biHeight; y++) {
        for (int x = 0; x < infoHeader.biWidth; x++) {
            int index = y * infoHeader.biWidth + x;
            int rowIndex = y * rowSize + x * 3;
            data[rowIndex + 0] = decodedB[index] * divisor;
            data[rowIndex + 1] = decodedG[index] * divisor;
            data[rowIndex + 2] = decodedR[index] * divisor;
        }
    }

    FILE *outf = fopen(output, "wb");

    fwrite(&fileHeader.bfType, 2, 1, outf);
    fwrite(&fileHeader.bfSize, 4, 1, outf);
    fwrite(&fileHeader.bfReserved1, 2, 1, outf);
    fwrite(&fileHeader.bfReserved2, 2, 1, outf);
    fwrite(&fileHeader.bfOffBits, 4, 1, outf);
    fwrite(&infoHeader, sizeof(infoHeader), 1, outf);

    
    for(int y = 0; y < infoHeader.biHeight; y++){
        fwrite(data + y*rowSize, 1, rowSize, outf);
    }

    fclose(outf);
    
    return 0;
}