#pragma once
// genere par tools/make_art.py : ne pas modifier a la main
#include <cstdint>

namespace ssl
{
    constexpr float kW = 1600.0f, kH = 1400.0f;   // repere de l'interface (100 %)
    struct Txt { float x, y; const char* s; float size; int weight; uint32_t argb; float track; bool left; };
    inline const Txt* texts (int& n)
    {
        static const Txt t[] = {
            { 346.00f, 464.00f, "VU", 13.00f, 1, 0xffa0eeb0u, 0.00f, false },
            { 150.00f, 328.00f, "SPACE SONAR", 36.00f, 2, 0xff141616u, 2.50f, true },
            { 482.00f, 330.00f, "209", 36.00f, 3, 0xff141616u, 1.50f, true },
            { 200.00f, 466.00f, "PEAK", 10.00f, 1, 0xffeceeeau, 1.20f, false },
            { 200.00f, 479.00f, "LEVEL", 10.00f, 1, 0xffeceeeau, 1.20f, false },
            { 190.00f, 540.00f, "MIC", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 190.00f, 554.00f, "VOLUME", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 345.00f, 540.00f, "FROM P.A.", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 345.00f, 554.00f, "VOLUME", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 500.00f, 540.00f, "INSTRUMENT", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 500.00f, 554.00f, "VOLUME", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 689.00f, 404.00f, "MODE SELECTOR", 12.50f, 1, 0xffeceeeau, 1.40f, false },
            { 653.50f, 589.49f, "1", 11.50f, 1, 0xffeceeeau, 0.00f, false },
            { 627.51f, 563.50f, "2", 11.50f, 1, 0xffeceeeau, 0.00f, false },
            { 618.00f, 528.00f, "3", 11.50f, 1, 0xffeceeeau, 0.00f, false },
            { 627.51f, 492.50f, "4", 11.50f, 1, 0xffeceeeau, 0.00f, false },
            { 653.50f, 466.51f, "5", 11.50f, 1, 0xffeceeeau, 0.00f, false },
            { 689.00f, 457.00f, "6", 11.50f, 1, 0xffeceeeau, 0.00f, false },
            { 724.50f, 466.51f, "7", 11.50f, 1, 0xffeceeeau, 0.00f, false },
            { 750.49f, 492.50f, "8", 11.50f, 1, 0xffeceeeau, 0.00f, false },
            { 760.00f, 528.00f, "9", 11.50f, 1, 0xffeceeeau, 0.00f, false },
            { 750.49f, 563.50f, "10", 11.50f, 1, 0xffeceeeau, 0.00f, false },
            { 724.50f, 589.49f, "11", 11.50f, 1, 0xffeceeeau, 0.00f, false },
            { 689.00f, 608.00f, "REVERB", 10.00f, 1, 0xffeceeeau, 1.20f, false },
            { 689.00f, 621.00f, "ONLY", 10.00f, 1, 0xffeceeeau, 1.20f, false },
            { 882.00f, 418.00f, "BASS", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 1020.00f, 418.00f, "TREBLE", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 1158.00f, 418.00f, "REVERB VOLUME", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 882.00f, 552.00f, "REPEAT RATE", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 1020.00f, 552.00f, "INTENSITY", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 1158.00f, 552.00f, "ECHO VOLUME", 11.00f, 1, 0xffeceeeau, 1.10f, false },
            { 1362.00f, 548.00f, "POWER", 11.00f, 1, 0xffeceeeau, 1.20f, false },
            { 1412.00f, 584.00f, "ON", 9.50f, 1, 0xffeceeeau, 1.00f, false },
            { 1412.00f, 660.00f, "OFF", 9.50f, 1, 0xffeceeeau, 1.00f, false },
            { 198.00f, 712.00f, "MIC", 10.50f, 1, 0xff141616u, 1.10f, false },
            { 353.00f, 712.00f, "FROM P.A.", 10.50f, 1, 0xff141616u, 1.10f, false },
            { 452.00f, 712.00f, "INSTRUMENT", 10.50f, 1, 0xff141616u, 1.10f, true },
            { 1014.00f, 712.00f, "OUTPUT", 10.50f, 1, 0xff141616u, 1.10f, true },
            { 1266.00f, 712.00f, "ECHO CANCEL", 10.50f, 1, 0xff141616u, 1.10f, true },
            { 600.00f, 764.00f, "ECHO", 8.50f, 1, 0xff141616u, 0.80f, false },
            { 672.00f, 764.00f, "NORMAL", 8.50f, 1, 0xff141616u, 0.80f, false },
            { 774.00f, 712.00f, "MIC", 10.50f, 1, 0xff141616u, 0.80f, false },
            { 821.00f, 712.00f, "P.A.", 10.50f, 1, 0xff141616u, 0.80f, false },
            { 868.00f, 712.00f, "INST", 10.50f, 1, 0xff141616u, 0.80f, false },
            { 1040.00f, 730.00f, "H", 9.50f, 1, 0xff141616u, 0.00f, false },
            { 1066.00f, 730.00f, "M", 9.50f, 1, 0xff141616u, 0.00f, false },
            { 1092.00f, 730.00f, "L", 9.50f, 1, 0xff141616u, 0.00f, false },
            { 1380.00f, 764.00f, "ON   \xc2\xb7   OFF", 8.50f, 1, 0xff141616u, 0.80f, false },
            { 425.00f, 975.00f, "BLOCK DIAGRAM", 11.00f, 1, 0xffc4cac6u, 1.30f, false },
            { 905.00f, 1110.00f, "STEREO MODE", 10.50f, 1, 0xffc4cac6u, 1.00f, false },
            { 1120.00f, 1010.00f, "RATE RANGE", 10.50f, 1, 0xffc4cac6u, 1.00f, false },
            { 1120.00f, 1210.00f, "SPEED", 10.00f, 1, 0xffc4cac6u, 1.00f, false },
            { 1410.00f, 1010.00f, "TAPE SPEED LFO", 10.50f, 1, 0xffc4cac6u, 1.00f, false },
            { 1310.00f, 1105.00f, "RATE", 10.00f, 1, 0xffc4cac6u, 1.00f, false },
            { 1410.00f, 1105.00f, "WAVEFORM", 10.00f, 1, 0xffc4cac6u, 1.00f, false },
            { 1510.00f, 1105.00f, "DEPTH", 10.00f, 1, 0xffc4cac6u, 1.00f, false },
            { 905.00f, 1268.00f, "CONDITION", 10.00f, 1, 0xffc4cac6u, 1.00f, false },
            { 1010.00f, 1268.00f, "W/F", 10.00f, 1, 0xffc4cac6u, 1.00f, false },
            { 1120.00f, 1268.00f, "MOTOR TORQUE", 10.00f, 1, 0xffc4cac6u, 1.00f, false },
            { 1265.00f, 1268.00f, "LOW CUT", 10.00f, 1, 0xffc4cac6u, 1.00f, false },
            { 1385.00f, 1268.00f, "HIGH CUT", 10.00f, 1, 0xffc4cac6u, 1.00f, false },
            { 1510.00f, 1268.00f, "MASTER", 10.00f, 1, 0xffc4cac6u, 1.00f, false },
        };
        n = (int) (sizeof (t) / sizeof (t[0]));
        return t;
    }
}
