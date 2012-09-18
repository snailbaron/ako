#include <cstdio>
#include <cstring>
#include <cstdint>

uint8_t bitBuffer = 0;
int bitsInBuffer = 0;
int bitsInStorage = 0;
uint32_t low = 0;
uint32_t high = uint16_t(-1);

void PushZeroBit(FILE *outfile)
{
    bitBuffer <<= 1;
    bitsInBuffer++;

    if (bitsInBuffer == 8)
    {
        fwrite(&bitBuffer, 1, 1, outfile);
        bitsInBuffer = 0;
    }
}

void PushUnitBit(FILE *outfile)
{
    bitBuffer = (bitBuffer << 1) + 1;
    bitsInBuffer++;

    if (bitsInBuffer == 8)
    {
        fwrite(&bitBuffer, 1, 1, outfile);
        bitsInBuffer = 0;
    }   
}

bool PullBit(FILE *infile, int *bit)
{
    if (bitsInBuffer == 0)
    {
        if (!fread_s(&bitBuffer, 1, 1, 1, infile))
            return false;
                
        bitsInBuffer = 8;
    }
    
    *bit = (1 & bitBuffer);
    bitBuffer >>= 1;
    bitsInBuffer--;
    
    return true;
}

void Stretch(int l, int r, int d, FILE *outfile)
{
    low = low + (high - low) * l / d;
    high = low + (high - low) * r / d;

    bool stretching = true;
    while (stretching)
    {
        const int SECOND_QRT_START = (1 << 14);
        const int THIRD_QRT_START = (1 << 15);
        const int FOURTH_QRT_START = (3 << 14);

        if (high < SECOND_QRT_START)
        {
            PushZeroBit(outfile);
            low = 2 * low;
            high = 2 * high + 1;

            for ( ; bitsInStorage > 0; bitsInStorage--)
                PushUnitBit(outfile);
        }
        else if (low >= THIRD_QRT_START)
        {
            PushUnitBit(outfile);
            low = 2 * (low - THIRD_QRT_START);
            high = 2 * (high - THIRD_QRT_START) + 1;

            for ( ; bitsInStorage > 0; bitsInStorage--)
                PushZeroBit(outfile);    
        }
        else if (low >= SECOND_QRT_START && high < FOURTH_QRT_START)
        {
            bitsInStorage++;
            low = 2 * (low - SECOND_QRT_START);
            high = 2 * (high - SECOND_QRT_START);
        }
    }
}   

int main(int argc, char *argv[])
{
    // COMPRESS
    FILE *infile = 0, *outfile = 0;
    fopen_s(&infile, "infile.txt", "rb");
    fopen_s(&outfile, "outfile.txt", "wb");

    const char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    int symCount = strlen(ALPHABET);

    char *symbols = new char[symCount + 1];
    uint16_t *freqs = new uint16_t[symCount + 1];
    uint16_t *cumFreqs = new uint16_t[symCount + 2];
    for (int i = 0; i < symCount; i++)
        symbols[i] = ALPHABET[i];
    for (int i = 0; i <= symCount; i++)
        freqs[i] = 1;
    cumFreqs[0] = 0;
    for (int i = 0; i <= symCount; i++)
        cumFreqs[i+1] = cumFreqs[i] + freqs[i];
    
    int symbolCode;
    while ((symbolCode = getc(infile)) != EOF)
    {
        char sym = (char) symbolCode;

        int idx;
        for (idx = 0; idx < symCount && symbols[idx] != sym; idx++);
        if (idx < symCount)
        {
            int l = cumFreqs[idx];
            int r = cumFreqs[idx+1];
            int d = cumFreqs[symCount + 1];

            Stretch(l, r, d, outfile);
        }
    }
    if (feof(infile))
        printf("Input file was successfully read to the end\n");
    if (ferror(infile))
        printf("Error while reading the input file\n");

    Stretch(cumFreqs[symCount], cumFreqs[symCount + 1], cumFreqs[symCount + 1], outfile);

    for (int flag = (1 << 15); flag > 0 && ((low ^ high) & flag); flag >>= 1)
    {
        if (low & flag)
            PushUnitBit(outfile);
        else
            PushZeroBit(outfile);
    }

    delete[] cumFreqs;
    delete[] freqs;
    delete[] symbols;
    
    fclose(outfile);
    fclose(infile);


    // DECOMPRESS
    FILE *code = 0, *decode = 0;
    fopen_s(&code, "outfile.txt", "rb");
    fopen_s(&decode, "decoded.txt", "wb");

    bitBuffer = 0;
    bitsInBuffer = 0;

    low = 0;
    high = uint16_t(-1);

    int bit;
    while (PullBit(code, &bit))
    {
        
    }
    if (feof(code))
        printf("Code file successfully read to the end\n");
    if (ferror(code))
        printf("Error while reading the code file\n");



    fclose(decode);
    fclose(code);

    return 0;
}