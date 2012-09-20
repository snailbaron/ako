#include "ari_codec.hpp"
#include <conio.h>

int main(int argc, char *argv[])
{
    AriCodec codec;

    printf("Beginning compression\n");
    codec.Compress("infile.txt", "outfile.txt");
    printf("\n");

    printf("Beginning decompression\n");
    codec.Decompress("outfile.txt", "decoded.txt");

    printf("Press any key...\n");
    _getch();

    return 0;
}