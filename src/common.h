#ifndef _SFMCOMMON_H                       // duplication check
#define _SFMCOMMON_H

#ifdef _MSC_VER
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

#include <iostream>
#include <algorithm>
#include <iomanip>
#include <string>
#include <kcutil.h>
#include <ktutil.h>
#include <tr1/memory>

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

using namespace std;
namespace superfastmatch{
  namespace kt=kyototycoon;
  namespace kc=kyotocabinet;

  // Global Forward Declarations
  class Document;
  class Association;
  class Command;

  // Global Typedefs
  typedef uint32_t hash_t;
  typedef std::tr1::shared_ptr<Document> DocumentPtr;
  typedef std::tr1::shared_ptr<Association> AssociationPtr;
  typedef std::tr1::shared_ptr<Command> CommandPtr;

  // Global consts
  const uint64_t MAX_HASH=(1L<<32)-1;
  
  //Global utility functions
  inline hash_t hashmurmur(const void* buf, size_t size) {
    const uint64_t mul = 0xc6a4a7935bd1e995ULL;
    const int32_t rtt = 47;
    uint64_t hash = 19780211ULL ^ (size * mul);
    const unsigned char* rp = (const unsigned char*)buf;
    while (size >= sizeof(uint64_t)) {
      uint64_t num = ((uint64_t)rp[0] << 0) | ((uint64_t)rp[1] << 8) |
        ((uint64_t)rp[2] << 16) | ((uint64_t)rp[3] << 24) |
        ((uint64_t)rp[4] << 32) | ((uint64_t)rp[5] << 40) |
        ((uint64_t)rp[6] << 48) | ((uint64_t)rp[7] << 56);
      num *= mul;
      num ^= num >> rtt;
      num *= mul;
      hash *= mul;
      hash ^= num;
      rp += sizeof(uint64_t);
      size -= sizeof(uint64_t);
    }
    switch (size) {
    case 7: hash ^= (uint64_t)rp[6] << 48;
    case 6: hash ^= (uint64_t)rp[5] << 40;
    case 5: hash ^= (uint64_t)rp[4] << 32;
    case 4: hash ^= (uint64_t)rp[3] << 24;
    case 3: hash ^= (uint64_t)rp[2] << 16;
    case 2: hash ^= (uint64_t)rp[1] << 8;
    case 1: hash ^= (uint64_t)rp[0];
      hash *= mul;
    };
    hash ^= hash >> rtt;
    hash *= mul;
    hash ^= hash >> rtt;
    return hash;
  }
  
  //Totally ripped from kctreemgr.cc!	
  inline void oprintf(std::ostream& s,const char* format, ...) {
    std::string msg;
    va_list ap;
    va_start(ap, format);
    kc::vstrprintf(&msg, format, ap);
    va_end(ap);
    s << msg;
  }

  inline std::string unitnumstr(int64_t num) {
    if (num >= std::pow(1000.0, 6)) {
      return kyotocabinet::strprintf("%.3Lf quintillion", (long double)num / std::pow(1000.0, 6));
    } else if (num >= std::pow(1000.0, 5)) {
      return kyotocabinet::strprintf("%.3Lf quadrillion", (long double)num / std::pow(1000.0, 5));
    } else if (num >= std::pow(1000.0, 4)) {
      return kyotocabinet::strprintf("%.3Lf trillion", (long double)num / std::pow(1000.0, 4));
    } else if (num >= std::pow(1000.0, 3)) {
      return kyotocabinet::strprintf("%.3Lf billion", (long double)num / std::pow(1000.0, 3));
    } else if (num >= std::pow(1000.0, 2)) {
      return kyotocabinet::strprintf("%.3Lf million", (long double)num / std::pow(1000.0, 2));
    } else if (num >= std::pow(1000.0, 1)) {
      return kyotocabinet::strprintf("%.3Lf thousand", (long double)num / std::pow(1000.0, 1));
    }
    return kyotocabinet::strprintf("%lld", (long long)num);
  }


  // convert a number into the string with the byte unit
  inline std::string unitnumstrbyte(int64_t num) {
    if ((unsigned long long)num >= 1ULL << 60) {
      return kyotocabinet::strprintf("%.3Lf EiB", (long double)num / (1ULL << 60));
    } else if ((unsigned long long)num >= 1ULL << 50) {
      return kyotocabinet::strprintf("%.3Lf PiB", (long double)num / (1ULL << 50));
    } else if ((unsigned long long)num >= 1ULL << 40) {
      return kyotocabinet::strprintf("%.3Lf TiB", (long double)num / (1ULL << 40));
    } else if ((unsigned long long)num >= 1ULL << 30) {
      return kyotocabinet::strprintf("%.3Lf GiB", (long double)num / (1ULL << 30));
    } else if ((unsigned long long)num >= 1ULL << 20) {
      return kyotocabinet::strprintf("%.3Lf MiB", (long double)num / (1ULL << 20));
    } else if ((unsigned long long)num >= 1ULL << 10) {
      return kyotocabinet::strprintf("%.3Lf KiB", (long double)num / (1ULL << 10));
    }
    return kyotocabinet::strprintf("%lld B", (long long)num);
  }

  template <class C> void FreeClear( C & cntr ) {
    for ( typename C::iterator it = cntr.begin(); it != cntr.end(); ++it ) {
      if (*it!=0){
        delete *it;
        *it=0;
      }
    }
    cntr.clear();
  }
  
  inline bool isNumeric(string& input){
    float f; 
    istringstream s(input); 
    return(s >> f);
  }
  
  inline string toString(uint64_t number){ 
    stringstream s;
    s << number;
    return s.str();
  }
  
  inline bool notAlphaNumeric(char c){
    return std::isalnum(c)==0;
  }
  
  template <typename T, typename U>
  class create_map
  {
  private:
      std::map<T, U> m_map;
  public:
      create_map(const T& key, const U& val)
      {
          m_map[key] = val;
      }

      create_map<T, U>& operator()(const T& key, const U& val)
      {
          m_map[key] = val;
          return *this;
      }

      operator std::map<T, U>()
      {
          return m_map;
      }
  };
}

#endif
