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

class hte{
    public:
        int frq, val;
        hte *l, *r;
        hte(){
            frq = 0;
            l = r = NULL;
        }
};

class bitpattern{
    public:
        BYTE pattern[1024];
        int digit = 0;
        void writebit(BYTE bit){
            pattern[digit++] = bit;
        }
        void reset(){
            digit = 0;
        }
};

class bitarr{
    public:
        BYTE* data = NULL;
        int bitp = 0;
        void putbit(BYTE bit){
            int i = bitp / 8;
            int actual_bitp = bitp - i * 8;

            int shiftamount = 8 - 1 - actual_bitp;
            bit <<= shiftamount;
            data[i] |= bit;

            bitp++;
        }
        void putbitpattern(bitpattern &bitpat){
            for(int u = 0; u < bitpat.digit; u++){
                putbit(bitpat.pattern[u]);
            }
        }
        bitarr(int size){
            data = new BYTE[size];
        }
};

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

void add_huff(hte *huff[], BYTE c){
    if(huff[c] == NULL){
        huff[c] = new hte;
        huff[c]->val = c;
    }
    huff[c]->frq++;
}

void sort(hte **start, hte **end){
    int numElems = end - start;

    for(int i = 0; i < numElems - 1; i++){
        for(int j = 0; j < numElems - i - 1; j++){

            hte* a = start[j];
            hte* b = start[j+1];

            if(a == NULL && b != NULL){
                start[j+1] = a;
                start[j] = b;
                continue;
            }
            if(a != NULL && b != NULL){
                if(a->frq > b->frq){
                    start[j+1] = a;
                    start[j] = b;
                }
            }
        }
    }
}

bitpattern codesR[256], codesG[256], codesB[256];
void generate_codes(hte *node,  bitpattern codes[], bitpattern &current){
    if(!node) return;

    if(!node->l && !node->r){
        codes[node->val] = current;
        return;
    }
    
    current.writebit(0);
    generate_codes(node->l, codes, current);
    current.digit--;

    current.writebit(1);
    generate_codes(node->r, codes, current);
    current.digit--;
}

void build_huffman_tree(hte* huff[], bitpattern codes[], int divisor) {
    int active = 0;
    for (int i = 0; i < 256/divisor; i++)
        if (huff[i] != NULL) active++;

    int writeIndex = 0;
    for (int i = 0; i < active; i++) {
        if (huff[i] != NULL)
            huff[writeIndex++] = huff[i];
    }
    active = writeIndex;
    while (active > 1) {
        sort(huff, huff + active);

        hte* comb = new hte();
        comb->l = huff[0];
        comb->r = huff[1];
        comb->frq = huff[0]->frq + huff[1]->frq;
        comb->val = -1;

        huff[0] = comb;
        for (int i = 2; i < active; i++)
            huff[i - 1] = huff[i];
        huff[active - 1] = NULL;
        active--;
    }

    bitpattern temp;
    temp.digit = 0;
    generate_codes(huff[0], codes, temp);
}

void build_huffman_table(bitpattern codes[], HuffmanTable &table, int divisor) {
    table.numEntries = 0;

    for (int i = 0; i < 256/divisor; i++) {
        if (codes[i].digit > 0) {
            table.entries[table.numEntries].value = i;
            table.entries[table.numEntries].bitLength = codes[i].digit;

            DWORD packed = 0;
            for (int j = 0; j < codes[i].digit; j++) {
                packed <<= 1;
                packed |= codes[i].pattern[j] & 1;
            }
            table.entries[table.numEntries].code = packed;

            table.numEntries++;
        }
    }
}

int main(int argc, char *argv[]) {
    
    if(argc != 3){
        printf("Incorrect # of inputs");
        return 0;
    }

    char *input = argv[1];
    int quality = atoi(argv[2]);

    int divisor = 1;
    if (quality >= 9) divisor = 1;
    else if (quality >= 7) divisor = 8;
    else if (quality >= 5) divisor = 16;
    else if (quality >= 3) divisor = 32;
    else divisor = 64;


    FILE *inf = fopen(input, "rb");

    struct tagBITMAPFILEHEADER fileHeader;
    struct tagBITMAPINFOHEADER infoHeader;
    
    fread(&fileHeader.bfType, 2, 1, inf);
    fread(&fileHeader.bfSize, 4, 1, inf);
    fread(&fileHeader.bfReserved1, 2, 1, inf);
    fread(&fileHeader.bfReserved2, 2, 1, inf);
    fread(&fileHeader.bfOffBits, 4, 1, inf);
    fread(&infoHeader, sizeof(infoHeader), 1, inf);

    BYTE *data = (BYTE*)mmap(NULL, infoHeader.biSizeImage, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    fread(data, infoHeader.biSizeImage, 1, inf);

    fclose(inf);

    int padding = (4 - (infoHeader.biWidth * 3) % 4) % 4;
    int rowSize = infoHeader.biWidth * 3 + padding;

    int num_elements = 256 / divisor;
    hte* huffR[num_elements]; hte* huffG[num_elements]; hte* huffB[num_elements];
    for (int i = 0; i < num_elements; i++) huffR[i]=huffG[i]=huffB[i]=NULL;
    for (int i = 0; i < num_elements; i++){
        if(huffR[i] == NULL){
            huffR[i] = new hte();
            huffR[i]->val = i;
            huffR[i]->frq = 1;
        }
        if(huffG[i] == NULL){
            huffG[i] = new hte();
            huffG[i]->val = i;
            huffG[i]->frq = 1;
        }
        if(huffB[i] == NULL){
            huffB[i] = new hte();
            huffB[i]->val = i;
            huffB[i]->frq = 1;
        }
    }

    for(int y = 0; y < infoHeader.biHeight; y++){
        for (int x = 0; x < infoHeader.biWidth; x++) {
            int index = y * rowSize + x * 3;

            BYTE r = data[index + 2] / divisor;
            BYTE g = data[index + 1] / divisor;
            BYTE b = data[index + 0] / divisor;

            add_huff(huffR, r);
            add_huff(huffG, g);
            add_huff(huffB, b);
        }
    }
    build_huffman_tree(huffR, codesR, divisor);
    build_huffman_tree(huffG, codesG, divisor);
    build_huffman_tree(huffB, codesB, divisor);

    int total_bitsR = 0, total_bitsG = 0, total_bitsB = 0;
    for(int y = 0; y < infoHeader.biHeight; y++){
        for (int x = 0; x < infoHeader.biWidth; x++) {
            int index = y * rowSize + x * 3;
            BYTE r = data[index + 2] / divisor;
            BYTE g = data[index + 1] / divisor;
            BYTE b = data[index + 0] / divisor;
            total_bitsR += codesR[r].digit;
            total_bitsG += codesG[g].digit;
            total_bitsB += codesB[b].digit;
        }
    }

    bitarr encodedR((total_bitsR + 7) / 8);
    bitarr encodedG((total_bitsG + 7) / 8);
    bitarr encodedB((total_bitsB + 7) / 8);

    for(int y = 0; y < infoHeader.biHeight; y++){
        for (int x = 0; x < infoHeader.biWidth; x++) {
            int width = infoHeader.biWidth;
            int height = infoHeader.biHeight;
            int index = y * rowSize + x * 3;

            BYTE r = data[index + 2] / divisor;
            BYTE g = data[index + 1] / divisor;
            BYTE b = data[index + 0] / divisor;

            encodedR.putbitpattern(codesR[r]);
            encodedG.putbitpattern(codesG[g]);
            encodedB.putbitpattern(codesB[b]);
        }
    }

    HuffmanTable tableR, tableG, tableB;

    build_huffman_table(codesR, tableR, divisor);
    build_huffman_table(codesG, tableG, divisor);
    build_huffman_table(codesB, tableB, divisor);

    char outname[100];
    strcpy(outname, input);

    char* dot = strrchr(outname, '.');
    if(dot != NULL) {
        *dot = '\0';
    }

    strcat(outname, ".zzz");

    FILE* outf = fopen(outname, "wb");

    fwrite(&fileHeader.bfType, 2, 1, outf);
    fwrite(&fileHeader.bfSize, 4, 1, outf);
    fwrite(&fileHeader.bfReserved1, 2, 1, outf);
    fwrite(&fileHeader.bfReserved2, 2, 1, outf);
    fwrite(&fileHeader.bfOffBits, 4, 1, outf);
    fwrite(&infoHeader, sizeof(infoHeader), 1, outf);

    fwrite(&encodedR.bitp, sizeof(int), 1, outf);
    fwrite(&encodedG.bitp, sizeof(int), 1, outf);
    fwrite(&encodedB.bitp, sizeof(int), 1, outf);

    fwrite(encodedR.data, 1, (encodedR.bitp+7)/8, outf);
    fwrite(encodedG.data, 1, (encodedG.bitp+7)/8, outf);
    fwrite(encodedB.data, 1, (encodedB.bitp+7)/8, outf);

    fwrite(&tableR.numEntries, sizeof(int), 1, outf);
    fwrite(tableR.entries, sizeof(HuffmanEntry), tableR.numEntries, outf);
    fwrite(&tableG.numEntries, sizeof(int), 1, outf);
    fwrite(tableG.entries, sizeof(HuffmanEntry), tableG.numEntries, outf);
    fwrite(&tableB.numEntries, sizeof(int), 1, outf);
    fwrite(tableB.entries, sizeof(HuffmanEntry), tableB.numEntries, outf);

    fwrite(&divisor, sizeof(int), 1, outf);

    fclose(outf);

    munmap(data, infoHeader.biSizeImage);
    
    return 0;
}