
struct quheader
{
    uint8_t _size[3];
    uint8_t _type;
    unsigned int _dejavu;
};

typedef struct
{
    unsigned char peers[4][4];
} ExchangePublicPeers;

struct EntityRequest
{
    struct quheader H;
    uint8_t pubkey[32];
};

struct Entity
{
    unsigned char publicKey[32];
    long long incomingAmount, outgoingAmount;
    unsigned int numberOfIncomingTransfers, numberOfOutgoingTransfers;
    unsigned int latestIncomingTransferTick, latestOutgoingTransferTick;
};

typedef struct
{
    struct Entity entity;
    unsigned int tick;
    int spectrumIndex;
    unsigned char siblings[SPECTRUM_DEPTH][32];
} RespondedEntity;

typedef struct
{
    unsigned short tickDuration;
    unsigned short epoch;
    unsigned int tick;
    unsigned short numberOfAlignedVotes;
    unsigned short numberOfMisalignedVotes;
    unsigned int initialTick;
} CurrentTickInfo;
