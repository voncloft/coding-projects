#include "../firmware/shared/wire.h"
size_t encode_frame(uint8_t t,uint8_t s,const uint8_t *p,uint16_t n,uint8_t *out){return wire_encode(t,s,p,n,out);}
int parse_stream(const uint8_t *p,size_t n,wire_packet *out,unsigned *bad){wire_parser parser={0};int count=0;for(size_t i=0;i<n;i++)if(wire_feed(&parser,p[i],out))count++;*bad=parser.bad;return count;}
