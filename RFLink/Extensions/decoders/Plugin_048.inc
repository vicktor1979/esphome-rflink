// v0.1.7 adapted extract. Only audited V2/V3 temperature/humidity families.
// Original header below is retained for provenance, NOT a claim that all listed models work.
// Modern output/checksum formulas cross-checked with RFLink32 Plugin_048.c.
//#######################################################################################################
//##                    This Plugin is only for use with the RFLink software package                   ##
//##                                    Plugin-48: Oregon V1/2/3                                       ##
//#######################################################################################################
/*********************************************************************************************\
 * This protocol takes care of receiving Oregon Scientific outdoor sensors that use the V1, V2 and V3 protocol
 *
 * models: THC238, THC268, THN132N, THWR288A, THRN122N, THN122N, AW129, AW131, THGR268, THGR122X,
 *         THGN122N, THGN123N, THGR122NX, THGR228N, THGR238, WTGR800, THGR918, THGRN228NX, THGN500,
 *         THGR810, RTGR328N, THGR328N, Huger BTHR918, BTHR918N, BTHR968, RGR126, RGR682, RGR918, PCR122
 *         THWR800, THR128, THR138, THC138, OWL CM119, cent-a-meter, OWL CM113, Electrisave, RGR928 
 *         UVN128, UV138, UVN800, Huger-STR918, WGR918, WGR800, PCR800, WGTR800, RGR126, BTHG968
 *
 * Author  (present)  : StormTeam 2018..2020 - Marc RIVES (aka Couin3)
 * Support (present)  : https://github.com/couin3/RFLink 
 * Author  (original) : StuntTeam 2015..2016
 * Support (original) : http://sourceforge.net/projects/rflink/
 * License            : This code is free for use in any open source project when this header is included.
 *                      Usage of any parts of this code in a commercial application is prohibited!  
 *********************************************************************************************
 * Changelog: v0.1 beta 
 *********************************************************************************************
 * Technical information:
 * Supports Oregon V1, V2 and V3 protocol messages
 * Core code from https://github.com/Cactusbone/ookDecoder/blob/master/ookDecoder.ino
 * Copyright (c) 2014 Charly Koza cactusbone@free.fr Copyright (c) 2012 Olivier Lebrun olivier.lebrun@connectingstuff.net 
 * Copyright (c) 2012 Dominique Pierre (zzdomi) Copyright (c) 2010 Jean-Claude Wippler jcw@equi4.com
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, 
 * modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software 
 * is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 \*********************************************************************************************/

namespace rf_ext { namespace oregon {
class DecodeOOK
{
protected:
   byte total_bits, bits, flip, state, pos, data[25];
   virtual signed char decode(uint16_t width) = 0;

public:
   enum
   {
      UNKNOWN,
      T0,
      T1,
      T2,
      T3,
      OK,
      DONE
   };
   // -------------------------------------
   DecodeOOK() { resetDecoder(); }
   // -------------------------------------
   bool nextPulse(uint16_t width)
   {
      if (state != DONE)

         switch (decode(width))
         {
         case -1:
            resetDecoder();
            break;
         case 1:
            done();
            break;
         }
      return isDone();
   }
   // -------------------------------------
   bool isDone() const { return state == DONE; }
   // -------------------------------------
   const byte *getData(byte &count) const
   {
      count = pos;
      return data;
   }
   // -------------------------------------
   void resetDecoder()
   {
      total_bits = bits = pos = flip = 0;
      memset(data, 0, sizeof(data));
      state = UNKNOWN;
   }
   // -------------------------------------
   // add one bit to the packet data buffer
   // -------------------------------------
   virtual void gotBit(char value)
   {
      total_bits++;
      byte *ptr = data + pos;
      *ptr = (*ptr >> 1) | (value << 7);

      if (++bits >= 8)
      {
         bits = 0;
         if (++pos >= sizeof data)
         {
            resetDecoder();
            return;
         }
      }
      state = OK;
   }
   // -------------------------------------
   // store a bit using Manchester encoding
   // -------------------------------------
   void manchester(char value)
   {
      flip ^= value; // manchester code, long pulse flips the bit
      gotBit(flip);
   }
   // -------------------------------------
   // move bits to the front so that all the bits are aligned to the end
   // -------------------------------------
   void alignTail(byte max = 0)
   {
      // align bits
      if (bits != 0)
      {
         data[pos] >>= 8 - bits;
         for (byte i = 0; i < pos; ++i)
            data[i] = (data[i] >> bits) | (data[i + 1] << (8 - bits));
         bits = 0;
      }
      // optionally shift bytes down if there are too many of 'em
      if (max > 0 && pos > max)
      {
         byte n = pos - max;
         pos = max;
         for (byte i = 0; i < pos; ++i)
            data[i] = data[i + n];
      }
   }
   // -------------------------------------
   void reverseBits()
   {
      for (byte i = 0; i < pos; ++i)
      {
         byte b = data[i];
         for (byte j = 0; j < 8; ++j)
         {
            data[i] = (data[i] << 1) | (b & 1);
            b >>= 1;
         }
      }
   }
   // -------------------------------------
   void reverseNibbles()
   {
      for (byte i = 0; i < pos; ++i)
         data[i] = (data[i] << 4) | (data[i] >> 4);
   }
   // -------------------------------------
   void done()
   {
      while (bits)
         gotBit(0); // padding
      state = DONE;
   }
};

class OregonDecoderV2 : public DecodeOOK
{
public:
   OregonDecoderV2() {}

   // -------------------------------------
   // add one bit to the packet data buffer
   // -------------------------------------
   virtual void gotBit(char value)
   {
      if (!(total_bits & 0x01))
      {
         data[pos] = (data[pos] >> 1) | (value ? 0x80 : 00);
      }
      total_bits++;
      pos = total_bits >> 4;
      if (pos >= sizeof data)
      {
         resetDecoder();
         return;
      }
      state = OK;
   }
   // -------------------------------------
   virtual signed char decode(uint16_t width)
   {
      //if (200 <= width && width < 1200) {
      //	byte w = width >= 675;
      if (150 <= width && width < 1200)
      {
         byte w = width >= 600;
         switch (state)
         {
         case UNKNOWN:
            if (w != 0)
            {
               // Long pulse
               ++flip;
            }
            else if (w == 0 && 24 <= flip)
            {
               // Short pulse, start bit
               flip = 0;
               state = T0;
            }
            else
            {
               // Reset decoder
               return -1;
            }
            break;
         case OK:
            if (w == 0)
            {
               // Short pulse
               state = T0;
            }
            else
            {
               // Long pulse
               manchester(1);
            }
            break;
         case T0:
            if (w == 0)
            {
               // Second short pulse
               manchester(0);
            }
            else
            {
               // Reset decoder
               return -1;
            }
            break;
         }
      }
      else if (width >= 2500 && pos >= 8)
      {
         return 1;
      }
      else
      {
         return -1;
      }
      return total_bits == 160 ? 1 : 0;
   }
};

class OregonDecoderV3 : public DecodeOOK
{
public:
   OregonDecoderV3() {}
   // -------------------------------------
   // add one bit to the packet data buffer
   // -------------------------------------
   virtual void gotBit(char value)
   {
      data[pos] = (data[pos] >> 1) | (value ? 0x80 : 00);
      total_bits++;
      pos = total_bits >> 3;
      if (pos >= sizeof data)
      {
         resetDecoder();
         return;
      }
      state = OK;
   }
   // -------------------------------------
   virtual signed char decode(uint16_t width)
   {
      if (200 <= width && width < 1200)
      {
         byte w = width >= 675;
         switch (state)
         {
         case UNKNOWN:
            if (w == 0)
               ++flip;
            else if (32 <= flip)
            {
               flip = 1;
               manchester(1);
            }
            else
               return -1;
            break;
         case OK:
            if (w == 0)
               state = T0;
            else
               manchester(1);
            break;
         case T0:
            if (w == 0)
               manchester(0);
            else
               return -1;
            break;
         }
      }
      else
      {
         return -1;
      }
      return total_bits == 80 ? 1 : 0;
   }
};


// The message has an extra nibble compared with rtl_433's aligned payload.
// Use the RFLink representation and checksum, not rtl_433's byte indexes.
bool packet(const uint8_t *b, unsigned n) {
  if(n<8)return false;
  const unsigned model=(unsigned(b[0])<<8)|b[1];
  const bool temp_only=model==0xEA4C || model==0xCA48 || model==0x0A4D;
  const bool th=model==0xFA28 || model==0x1A2D || model==0x1A3D || model==0xCA2C ||
                model==0xFAB8 || (model&0xFFF)==0xACC;
  if(!temp_only && !th)return false;
  uint8_t sum=0;
  if(temp_only) {
    for(unsigned i=0;i<6;++i)sum=uint8_t(sum+(b[i]>>4)+(b[i]&15));
    sum=uint8_t(sum+(b[6]&15)-10);
    if(sum!=uint8_t(((b[7]&15)<<4)|(b[6]>>4)))return false;
  } else {
    if(n<9)return false;
    for(unsigned i=0;i<8;++i)sum=uint8_t(sum+(b[i]>>4)+(b[i]&15));
    if(uint8_t(sum-10)!=b[8])return false;
  }
  if((b[5]>>4)>9||(b[5]&15)>9||(b[4]>>4)>9)return false;
  const unsigned mag=(b[5]>>4)*100+(b[5]&15)*10+(b[4]>>4);
  const int t=(b[6]&8)?-int(mag):int(mag);
  const unsigned humidity=(b[7]&15)*10+(b[6]>>4);
  if(th && ((b[7]&15)>9 || (b[6]>>4)>9 || humidity>100))return false;
  if(repeated(48,signature(b,temp_only?8:9)))return true;
  display_Header();protocol_name("Oregon",model);
  // Combined rolling code/channel avoids merging different channels in NAME+ID packages.
  display_IDn((((b[2]&15)|(b[3]&0xF0))<<8)|(b[2]>>4),4);
  channel(b[2]>>4);display_TEMP(temperature(t));
  if(th)display_HUM(humidity,HUM_HEX);
  display_BAT(temp_only?((b[3]&8)==0):((b[4]&15)<4));
  display_Footer();return true;
}
} // namespace oregon
bool decode_048(const Pulses &p) {
  if(p.size()<100)return false;
  oregon::OregonDecoderV2 v2;
  oregon::OregonDecoderV3 v3;
  for(size_t i=0;i<p.size();++i) {
    const uint32_t width=p[i];
    if(width>65535){v2.resetDecoder();v3.resetDecoder();continue;}
    if(v2.nextPulse(static_cast<uint16_t>(width))) {
      byte count=0;const byte *data=v2.getData(count);
      if(oregon::packet(data,count))return true;
      v2.resetDecoder();
    }
    if(v3.nextPulse(static_cast<uint16_t>(width))) {
      byte count=0;const byte *data=v3.getData(count);
      if(oregon::packet(data,count))return true;
      v3.resetDecoder();
    }
  }
  return false;
}
} // namespace rf_ext
