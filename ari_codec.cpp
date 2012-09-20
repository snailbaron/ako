#include "ari_codec.hpp"

#include <cstdio>
#include <cstring>
#include <cstdint>

const char AriCodec::ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789\r\n";

AriCodec::AriCodec() :
    m_bitBuffer(0),
    m_bitsInBuffer(0),
    m_bitsInStorage(0),
    m_symbols(0),
    m_freqs(0),
    m_cumFreqs(0),
    m_symCount(0)
{
}

AriCodec::~AriCodec()
{
    if (m_symbols) delete[] m_symbols;
    if (m_freqs) delete[] m_freqs;
    if (m_cumFreqs) delete[] m_cumFreqs;    
}

void AriCodec::PushZeroBit(FILE *outfile)
{
    m_bitBuffer <<= 1;
    m_bitsInBuffer++;

    if (m_bitsInBuffer == 8)
    {
        printf("Pushing a byte: 0x%x\n", m_bitBuffer);
        fwrite(&m_bitBuffer, 1, 1, outfile);
        m_bitsInBuffer = 0;
    }
}

void AriCodec::PushUnitBit(FILE *outfile)
{
    m_bitBuffer = (m_bitBuffer << 1) + 1;
    m_bitsInBuffer++;

    if (m_bitsInBuffer == 8)
    {
        printf("Pushing a byte: 0x%x\n", m_bitBuffer);
        fwrite(&m_bitBuffer, 1, 1, outfile);
        m_bitsInBuffer = 0;
    }   
}

bool AriCodec::PullBit(FILE *infile, int *bit)
{
    if (m_bitsInBuffer == 0)
    {
        if (!fread_s(&m_bitBuffer, 1, 1, 1, infile))
            return false;

        printf("Pulled a byte: 0x%x\n", m_bitBuffer);
                
        m_bitsInBuffer = 8;
    }
    
    *bit = (1 & m_bitBuffer);
    m_bitBuffer >>= 1;
    m_bitsInBuffer--;
    
    return true;
}

void AriCodec::Stretch(uint32_t *lowPtr, uint32_t *highPtr, int l, int r, int d, FILE *outfile)
{
    uint32_t &low = *lowPtr;
    uint32_t &high = *highPtr;

    printf("Performing interval change\n");

    printf("  Cut: [ %d; %d ] ", low, high);

    low = low + (high - low) * l / d;
    high = low + (high - low) * r / d;

    printf("---> [ %d; %d ]\n", low, high);

    bool stretching = true;
    while (stretching)
    {
        const int SECOND_QRT_START = (1 << 14);
        const int THIRD_QRT_START = (1 << 15);
        const int FOURTH_QRT_START = (3 << 14);

        if (high < SECOND_QRT_START)
        {
            if (outfile)
                PushZeroBit(outfile);

            printf("  Stretch from left side: [ %d; %d ] ", low, high);
            low = 2 * low;
            high = 2 * high + 1;
            printf("---> [ %d; %d ]\n", low, high);

            printf("  Pushed: 0");

            if (outfile)
            {
                for ( ; m_bitsInStorage > 0; m_bitsInStorage--)
                {
                    PushUnitBit(outfile);
                    printf(" 1");
                }
            }

            printf("\n");
        }
        else if (low >= THIRD_QRT_START)
        {
            if (outfile)
                PushUnitBit(outfile);

            printf("  Stretch from right side: [ %d; %d ]", low, high);
            low = 2 * (low - THIRD_QRT_START);
            high = 2 * (high - THIRD_QRT_START) + 1;
            printf("---> [ %d; %d ]\n", low, high);

            printf("  Pushed: 1");

            if (outfile)
            {
                for ( ; m_bitsInStorage > 0; m_bitsInStorage--)
                {
                    PushZeroBit(outfile);
                    printf(" 0");
                }
            }

            printf("\n");
        }
        else if (low >= SECOND_QRT_START && high < FOURTH_QRT_START)
        {
            m_bitsInStorage++;

            printf("  Stretch from the middle: [ %d; %d ] ", low, high);
            low = 2 * (low - SECOND_QRT_START);
            high = 2 * (high - SECOND_QRT_START);
            printf("---> [ %d; %d ]\n", low, high);
        }
        else
        {
            stretching = false;
        }
    }

    printf("Interval stretching finished\n");
}   

void AriCodec::Compress(const char *inFileName, const char *outFileName)
{
    FILE *infile = 0, *outfile = 0;
    fopen_s(&infile, inFileName, "rb");
    fopen_s(&outfile, outFileName, "wb");

    m_symCount = strlen(ALPHABET);

    m_symbols = new char[m_symCount + 1];
    m_freqs = new uint16_t[m_symCount + 1];
    m_cumFreqs = new uint16_t[m_symCount + 2];
    for (int i = 0; i < m_symCount; i++)
        m_symbols[i] = ALPHABET[i];
    for (int i = 0; i <= m_symCount; i++)
        m_freqs[i] = 1;
    m_cumFreqs[0] = 0;
    for (int i = 0; i <= m_symCount; i++)
        m_cumFreqs[i+1] = m_cumFreqs[i] + m_freqs[i];

    printf("%10s%10s%10s\n", "Symbol", "Freq", "CumFreq");
    for (int i = 0; i < m_symCount; i++)
        printf("%10c%10d%10d\n", m_symbols[i], m_freqs[i], m_cumFreqs[i]);
    printf("%10s%10d%10d\n", "<EOF>", m_freqs[m_symCount], m_cumFreqs[m_symCount]);
    printf("%30d\n", m_cumFreqs[m_symCount+1]);
    printf("\n");
    
    int symbolCode;
    uint32_t low = 0;
    uint32_t high = uint16_t(-1);
    while ((symbolCode = getc(infile)) != EOF)
    {
        char sym = (char) symbolCode;

        int idx = 0;
        while (idx < m_symCount && m_symbols[idx] != sym)
            idx++;

        if (idx < m_symCount)
        {
            if (sym >= 32)
                printf("Coding symbol: %c (code %d)\n", sym, sym);
            else
                printf("Coding symbol (code %d)\n", sym);

            int l = m_cumFreqs[idx];
            int r = m_cumFreqs[idx+1];
            int d = m_cumFreqs[m_symCount + 1];

            printf("Frequency range for the symbol: [ %d; %d ]\n", l, r);
            printf("Frequency divider: %d\n", d);

            Stretch(&low, &high, l, r, d, outfile);
        }
        else
        {
            fprintf(stderr, "Unknown symbol encountered (code %d)\n", sym);
        }
    }
    if (feof(infile))
        printf("Input file was successfully read to the end\n");
    if (ferror(infile))
        printf("Error while reading the input file\n");

    printf("Coding <EOF> using its frequency range\n");
    Stretch(&low, &high, m_cumFreqs[m_symCount], m_cumFreqs[m_symCount + 1], m_cumFreqs[m_symCount + 1], outfile);

    printf("L = 0x%x\n", low);
    printf("H = 0x%x\n", high);

    printf("Pushing the last bits:");
    for (int flag = (1 << 15); flag > 0 && ((~(low ^ high)) & flag); flag >>= 1)
    {
        if (low & flag)
        {
            printf(" 1");
            PushUnitBit(outfile);
        }
        else
        {
            printf(" 0");
            PushZeroBit(outfile);
        }
    }
    printf("\n");

    fclose(outfile);
    fclose(infile);
}

void AriCodec::Decompress(const char *inFileName, const char *outFileName)
{
    FILE *code = 0, *decode = 0;
    fopen_s(&code, inFileName, "rb");
    fopen_s(&decode, outFileName, "wb");

    m_bitBuffer = 0;
    m_bitsInBuffer = 0;

    uint32_t low = 0;
    uint32_t high = uint16_t(-1);

    int lowSym = 0;
    int highSym = m_symCount - 1;

    int l = 0;
    int r = high;
    int bit;
    while (PullBit(code, &bit))
    {
        printf("Pulled bit: %d\n", bit);

        printf("Current symbol range: [ %d; %d ] ", l, r);
        if (bit)
            l += (r + 1 - l) / 2;
        else
            r -= (r + 1 - l) / 2;
        printf("---> [ %d; %d ]\n", l, r);

        while (m_cumFreqs[lowSym + 1] <= l)
            lowSym++;
        while (m_cumFreqs[highSym] > r)
            highSym--;

        printf("Symbols from %d to %d fit this description\n", lowSym, highSym);

        if (lowSym == highSym)
        {
            if (m_symbols[lowSym] >= 32)
                printf("Decoded symbol: %c (code %d)\n", m_symbols[lowSym], m_symbols[lowSym]);
            else
                printf("Decoded symbol (code %d)\n", m_symbols[lowSym], m_symbols[lowSym]);

            fwrite(&m_symbols[lowSym], 1, 1, decode);

            printf("Now stretching the interval\n");
            Stretch(&low, &high, m_cumFreqs[lowSym], m_cumFreqs[lowSym+1], m_cumFreqs[m_symCount + 1], 0);

            l = 0;
            r = uint16_t(-1);
            printf("Possible symbol range restored to [ %d; %d ]\n", l, r);
        }
        
        printf("\n");
    }
    if (feof(code))
        printf("Code file successfully read to the end\n");
    if (ferror(code))
        printf("Error while reading the code file\n");

    fclose(decode);
    fclose(code);
}