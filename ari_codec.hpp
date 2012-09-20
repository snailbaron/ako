#ifndef _ARI_CODEC_HPP_
#define _ARI_CODEC_HPP_

#include <cstdio>
#include <cstdint>

class AriCodec
{
    public:
        AriCodec();
        ~AriCodec();

        void Compress(const char *inFileName, const char *outFileName);
        void Decompress(const char *inFileName, const char *outFileName);

    private:
        void Stretch(uint32_t *lowPtr, uint32_t *highPtr, int l, int r, int d, FILE *outfile);
        bool PullBit(FILE *infile, int *bit);
        void PushUnitBit(FILE *outfile);
        void PushZeroBit(FILE *outfile);

        uint8_t m_bitBuffer;
        int m_bitsInBuffer;
        int m_bitsInStorage;

        static const char ALPHABET[];
        char *m_symbols;
        uint16_t *m_freqs;
        uint16_t *m_cumFreqs;
        int m_symCount;
};

#endif