and(8)          g9<1>.wUD       g9<4>.wUD       524032D         { align16 };
and(16)         g4<1>D          g6<8,8,1>D      1D              { align1 compr };
and(8)          g10<1>.xD       g10<4>.xD       1D              { align16 };
and(16)         g6<1>UD         g10<8,8,1>UD    g8<8,8,1>UD     { align1 compr };
and.nz.f0.0(16) null<1>D        g6<8,8,1>UD     1D              { align1 compr };
and(16)         g4<1>D          g8<8,8,1>UD     1D              { align1 compr };
and(8)          g2<1>D          g2<8,8,1>UD     1D              { align1 };
and.nz.f0.0(8)  null<1>.xD      g9<4>.xUD       1D              { align16 };
and(16)         g12<1>UD        g2.4<0,1,0>UD   0x80000000UD    { align1 compr };
and.nz.f0.0(16) g110<1>D        g6<8,8,1>D      1D              { align1 compr };
and(8)          g17<1>.xUD      g1<0>.xUD       0x80000000UD    { align16 };
and.nz.f0.0(16) g6<1>D          g4<8,8,1>UD     1D              { align1 compr };
and(1)          g12<1>UD        f0<0,1,0>UW     0x0000000fUD    { align1 nomask };
and(8)          g5<1>.xUD       g1<0>.xUD       g1<0>.yUD       { align16 };
and(8)          g8<1>.xD        g7<4>.xUD       1D              { align16 };
and.nz.f0.0(8)  g6<1>.xD        g6<4>.xD        1D              { align16 };
and.nz.f0.0(1)  null<1>UD       g1.6<0,1,0>UD   0x04000000UD    { align1 };