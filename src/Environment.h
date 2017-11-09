/*
    IIP Environment Variable Class

    Copyright (C) 2006-2016 Ruven Pillay.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#ifndef _ENVIRONMENT_H
#define _ENVIRONMENT_H


/* Define some default values
 */
#define VERBOSITY 1
#define LOGFILE "/tmp/iipsrv.log"
#define MAX_IMAGE_CACHE_SIZE 10.0
#define MAX_HEADERS_IN_METADATA_CACHE 1000
#define FILENAME_PATTERN "_pyr_"
#define JPEG_QUALITY 75
#define MAX_CVT 5000
#define MAX_SAMPLE_SIZE 0
#define MAX_LAYERS 0
#define FILESYSTEM_PREFIX ""
#define WATERMARK ""
#define WATERMARK_PROBABILITY 1.0
#define WATERMARK_OPACITY 1.0
#define LIBMEMCACHED_SERVERS "localhost"
#define LIBMEMCACHED_TIMEOUT 86400  // 24 hours
#define DISABLE_PRIMARY_MEMCACHE 0 // disble memcache to diagnose performance or functional issues
#define INTERPOLATION 1
#define CORS "";
#define BASE_URL "";
#define CACHE_CONTROL "max-age=86400"; // 24 hours
#define ALLOW_UPSCALING true
#define OVERSAMPLING_FACTOR 1.0; // no oversampling is default
#define NO_FILTER_DEFINED -999;
#define RETAIN_SOURCE_ICC_PROFILE 0
#define IIIF_PREFIX ""; // e.g. /iiif

#include <string>


/// Class to obtain environment variables
class Environment {


 public:

  static int getVerbosity(){
    int loglevel = VERBOSITY;
    char *envpara = getenv( "VERBOSITY" );
    if( envpara ){
      loglevel = atoi( envpara );
      // If not a realistic level, set to zero
      if( loglevel < 0 ) loglevel = 0;
    }
    return loglevel;
  }


  static std::string getLogFile(){
    char* envpara = getenv( "LOGFILE" );
    if( envpara ) return std::string( envpara );
    else return LOGFILE;
  }


  static float getMaxImageCacheSize(){
    float max_image_cache_size = MAX_IMAGE_CACHE_SIZE;
    char* envpara = getenv( "MAX_IMAGE_CACHE_SIZE" );
    if( envpara ){
      max_image_cache_size = atof( envpara );
    }
    return max_image_cache_size;
  }

  static int getMaxHeadersInMetadataCache(){
    char* envpara = getenv( "MAX_HEADERS_IN_METADATA_CACHE" );
    int max_headers_metadata_cache_elems;
    if( envpara ){
      max_headers_metadata_cache_elems = atoi( envpara );
      if ( max_headers_metadata_cache_elems < 0 )
        max_headers_metadata_cache_elems = 0;
    }
    else
      max_headers_metadata_cache_elems = MAX_HEADERS_IN_METADATA_CACHE;

    return max_headers_metadata_cache_elems;
  }

  static std::string getFileNamePattern(){
    char* envpara = getenv( "FILENAME_PATTERN" );
    std::string filename_pattern;
    if( envpara ){
      filename_pattern = std::string( envpara );
    }
    else filename_pattern = FILENAME_PATTERN;

    return filename_pattern;
  }


  static int getJPEGQuality(){
    char* envpara = getenv( "JPEG_QUALITY" );
    int jpeg_quality;
    if( envpara ){
      jpeg_quality = atoi( envpara );
      if( jpeg_quality > 100 ) jpeg_quality = 100;
      if( jpeg_quality < 1 ) jpeg_quality = 1;
    }
    else jpeg_quality = JPEG_QUALITY;

    return jpeg_quality;
  }

  static int getRetainSourceICCProfile(){
    char* envpara = getenv( "RETAIN_SOURCE_ICC_PROFILE" );
    int retainICC;
    if ( envpara ) {
      retainICC = atoi( envpara );
      if ( retainICC != 1 )
        retainICC = 0;
    }
    else
      retainICC = RETAIN_SOURCE_ICC_PROFILE;

    return retainICC;
  }

  // If we want a system wide ICC profile that will be used for standardized color transformation
  /*  static unsigned char *getSystemICCProfile(long *profileSize){
    char* envpara = getenv( "ICC_PROFILE" );

    std::string iccfilename;
    if ( envpara )
        iccfilename = std::string( envpara );
    else
        iccfilename = ICC_PROFILE;

    // open the given icc profile and load into an unsigned char*
    if ( !iccfilename.empty() ) {
        FILE *f = fopen(iccfilename.c_str(), "rb");
        if ( f ) {
            fseek(f, 0, SEEK_END);
            *profileSize = ftell(f);
            fseek(f, 0, SEEK_SET);  // same as rewind(f);
            unsigned char *iccProfile = (unsigned char *) malloc(*profileSize);
            fread(iccProfile, *profileSize, 1, f);
            fclose(f);
            return iccProfile;
        }
    }
    return NULL;
  }
  */


  static int getMaxCVT(){
    char* envpara = getenv( "MAX_CVT" );
    int max_CVT;
    if( envpara ){
      max_CVT = atoi( envpara );
      if( max_CVT < 64 ) max_CVT = 64;
    }
    else max_CVT = MAX_CVT;

    return max_CVT;
  }

  static int getMaxSampleSize(){
    char* envpara = getenv( "MAX_SAMPLE_SIZE" );
    int max_sample_size;
    if( envpara ){
      max_sample_size = atoi( envpara );
      if ( max_sample_size < 0 ) max_sample_size = 0;
    }
    else max_sample_size = MAX_SAMPLE_SIZE;

    return max_sample_size;
  }


  static int getMaxLayers(){
    char* envpara = getenv( "MAX_LAYERS" );
    int layers;
    if( envpara ) layers = atoi( envpara );
    else layers = MAX_LAYERS;

    return layers;
  }


  static std::string getFileSystemPrefix(){
    char* envpara = getenv( "FILESYSTEM_PREFIX" );
    std::string filesystem_prefix;
    if( envpara ){
      filesystem_prefix = std::string( envpara );
    }
    else filesystem_prefix = FILESYSTEM_PREFIX;

    return filesystem_prefix;
  }


  static std::string getWatermark(){
    char* envpara = getenv( "WATERMARK" );
    std::string watermark;
    if( envpara ){
      watermark = std::string( envpara );
    }
    else watermark = WATERMARK;

    return watermark;
  }


  static float getWatermarkProbability(){
    float watermark_probability = WATERMARK_PROBABILITY;
    char* envpara = getenv( "WATERMARK_PROBABILITY" );

    if( envpara ){
      watermark_probability = atof( envpara );
      if( watermark_probability > 1.0 ) watermark_probability = 1.0;
      if( watermark_probability < 0 ) watermark_probability = 0.0;
    }

    return watermark_probability;
  }


  static float getWatermarkOpacity(){
    float watermark_opacity = WATERMARK_OPACITY;
    char* envpara = getenv( "WATERMARK_OPACITY" );

    if( envpara ){
      watermark_opacity = atof( envpara );
      if( watermark_opacity > 1.0 ) watermark_opacity = 1.0;
      if( watermark_opacity < 0 ) watermark_opacity = 0.0;
    }

    return watermark_opacity;
  }


  static std::string getMemcachedServers(){
    char* envpara = getenv( "MEMCACHED_SERVERS" );
    std::string memcached_servers;
    if( envpara ){
      memcached_servers = std::string( envpara );
    }
    else memcached_servers = LIBMEMCACHED_SERVERS;

    return memcached_servers;
  }


  static unsigned int getMemcachedTimeout(){
    char* envpara = getenv( "MEMCACHED_TIMEOUT" );
    unsigned int memcached_timeout;
    if( envpara ) memcached_timeout = atoi( envpara );
    else memcached_timeout = LIBMEMCACHED_TIMEOUT;

    return memcached_timeout;
  }

  static int getDisablePrimaryMemcache(){
    char* envpara = getenv( "DISABLE_PRIMARY_MEMCACHE" );
    int disableMem;
    if ( envpara ) {
      disableMem = atoi( envpara );
      if ( disableMem != 1 )
        disableMem = 0;
    }
    else
      disableMem = DISABLE_PRIMARY_MEMCACHE;

    return disableMem;
  }

  static unsigned int getInterpolation(){
    char* envpara = getenv( "INTERPOLATION" );
    unsigned int interpolation;
    if( envpara ) interpolation = atoi( envpara );
    else interpolation = INTERPOLATION;

    return interpolation;
  }


  static std::string getCORS(){
    char* envpara = getenv( "CORS" );
    std::string cors;
    if( envpara ) cors = std::string( envpara );
    else cors = CORS;
    return cors;
  }


  static std::string getBaseURL(){
    char* envpara = getenv( "BASE_URL" );
    std::string base_url;
    if( envpara ) base_url = std::string( envpara );
    else base_url = BASE_URL;
    return base_url;
  }


  static std::string getCacheControl(){
    char* envpara = getenv( "CACHE_CONTROL" );
    std::string cache_control;
    if( envpara ) cache_control = std::string( envpara );
    else cache_control = CACHE_CONTROL;
    return cache_control;
  }
  
  static bool getAllowUpscaling(){
    char* envpara = getenv( "ALLOW_UPSCALING" );
    bool allow_upscaling;
    if( envpara ) allow_upscaling =  atoi( envpara ); //implicit cast to boolean, all values other than '0' treated as true
    else allow_upscaling = ALLOW_UPSCALING;
    return allow_upscaling;
  }

  static float getOversamplingFactor(){
    float os = OVERSAMPLING_FACTOR;
    char *envpara = getenv( "OVERSAMPLING_FACTOR" );
    if( envpara ){
      os = atof( envpara );
      // If not a realistic oversampling factor, set to default
      if( os < 1 ) os = 1;
      if( os > 2 ) os = 2;
    }
    return os;
  }

  static std::string getIIIFPrefix(){
    char* envpara = getenv( "IIIF_PREFIX" );
    std::string iiif_prefix;
    if( envpara ) iiif_prefix = std::string( envpara );
    else iiif_prefix = IIIF_PREFIX;
    return iiif_prefix;
  }

#ifdef HAVE_PNG

  /****************************************
   from zlib.h
   #define Z_NO_COMPRESSION         0
   #define Z_BEST_SPEED             1
   #define Z_BEST_COMPRESSION       9
   #define Z_DEFAULT_COMPRESSION  (-1)
  ****************************************/
  static int getPNGCompressionLevel(){
    int png_compression_level = Z_NO_COMPRESSION;
    char *envval = getenv( "PNG_COMPRESSION_LEVEL" );
    if ( envval != NULL ) {
      string envpara = string(envval);
      if ( envpara.compare("Z_BEST_SPEED") )
        png_compression_level = Z_BEST_SPEED;
      else if ( envpara.compare("Z_BEST_COMPRESSION") )
        png_compression_level = Z_BEST_COMPRESSION;
      else if ( envpara.compare("Z_DEFAULT_COMPRESSION") )
        png_compression_level = Z_DEFAULT_COMPRESSION;
    }
    return png_compression_level;
  }

  /****************************************
   from png.h
   #define PNG_NO_FILTERS     0x00
   #define PNG_FILTER_NONE    0x08
   #define PNG_FILTER_SUB     0x10
   #define PNG_FILTER_UP      0x20
   #define PNG_FILTER_AVG     0x40
   #define PNG_FILTER_PAETH   0x80
   #define PNG_ALL_FILTERS (PNG_FILTER_NONE | PNG_FILTER_SUB | PNG_FILTER_UP | \
                            PNG_FILTER_AVG | PNG_FILTER_PAETH)
  ****************************************/
  static int PNGFilterTypeToInt( const char *filterType ) {
    int png_ftype = NO_FILTER_DEFINED;
    if ( filterType != NULL ) {
      string filter = string(filterType);
      if ( filter.compare("PNG_FILTER_NONE") )
        png_ftype = PNG_FILTER_NONE;
      else if ( filter.compare("PNG_FILTER_SUB") )
        png_ftype = PNG_FILTER_SUB;
      else if ( filter.compare("PNG_FILTER_UP") )
        png_ftype = PNG_FILTER_UP;
      else if ( filter.compare("PNG_FILTER_AVG") )
        png_ftype = PNG_FILTER_AVG;
      else if ( filter.compare("PNG_FILTER_PAETH") )
        png_ftype = PNG_FILTER_PAETH;
      else if ( filter.compare("PNG_ALL_FILTERS") )
        png_ftype = PNG_ALL_FILTERS;
    }
    return png_ftype;
  }

  static int getPNGFilterType() {
    int filterType = PNGFilterTypeToInt( getenv( "PNG_FILTER_TYPE" ) );
    int checkType = NO_FILTER_DEFINED;
    if ( filterType == checkType ) {
        filterType = PNG_NO_FILTERS;
    }
    return filterType;
  }

#endif // HAVE_PNG

};


#endif
