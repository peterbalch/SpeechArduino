const byte nBand = 4;
// frequencies: 150, 356, 843, 2000
const long filt_a0[nBand+1] = {3259, 7166, 14089, 20190};
const long filt_b1[nBand+1] = {-123690, -112214, -81133, 0};
const long filt_b2[nBand+1] = {59018, 51204, 37359, 25156};
