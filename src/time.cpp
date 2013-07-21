#include <omnimapper/time.h>

boost::posix_time::ptime omnimapper::stamp2ptime (uint64_t stamp)
{  
  uint64_t sec64 = 0;
  uint64_t nsec64 = stamp;
  
  uint64_t nsec_part = nsec64 % 1000000000UL;
  uint64_t sec_part = nsec64 / 1000000000UL;
  
  if (sec_part > UINT_MAX)
    printf ("ERROR: Time is out of 32-bit range!\n");
  
  sec64 += sec_part;
  nsec64 = nsec_part;
  
  uint32_t sec = (uint32_t)sec64;
  uint32_t nsec = (uint32_t)nsec64;
  
#if defined(BOOST_DATE_TIME_HAS_NANOSECONDS)
  return (boost::posix_time::from_time_t(sec) + boost::posix_time::nanoseconds(nsec));
#else
  return (boost::posix_time::from_time_t(sec) + boost::posix_time::microseconds(nsec/1000.0));
#endif
}