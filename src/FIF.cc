/*
    IIP FIF Command Handler Class Member Function

    Copyright (C) 2006-2015 Ruven Pillay.

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


#include <algorithm>
#include <sstream>
#include <bits/stdc++.h> 
#include <sys/stat.h>
#include "Task.h"
#include "URL.h"
#include "Environment.h"
#include "IIPImage.h"
#include "TPTImage.h"

#ifdef HAVE_KAKADU
#include "KakaduImage.h"
#endif

#ifdef HAVE_OPENJPEG
#include "OpenJPEGImage.h"
#endif

using namespace std;

const string SIZESEP = "__";


// returns a potentially modified maximum number of pixels that should be used
// when sampling this image
void FIF::run( Session* session, const string& src ){

  if( session->loglevel >= 3 ) 
    *(session->logfile) << "FIF handler reached" << endl;

  // Time this command
  if( session->loglevel >= 2 ) 
    command_timer.start();

  // Define our separator depending on the OS
#ifdef WIN32
  const string separator = "\\";
#else
  const string separator = "/";
#endif

  // Get our image pattern variable
  string filesystem_prefix = Environment::getFileSystemPrefix();

  // Decode any URL-encoded characters from our path
  URL url( src );
  string argument = url.decode();

  // special handling of regex for NGA that should be turned into a regex_replace general purpose routine 
  // based on configuration but for now is hard-coded - this one works with uuids to transform requests for uuids
  // into actual file name paths which are then used for loading the image file, but NOT used in URL responses back
  // to the client since we want to hide this complexity from the client

  //                $1              $2                                     $3                                        $4
  //              first 3        second 3                               rest of uuid                              optional size
  regex uuidre("^/?([a-z0-9]{3})([a-z0-9]{3})([a-z0-9]{2}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{4}-[a-z0-9]{12})(?:"+SIZESEP+"(.*))?");
  std::smatch sm;
  std::regex_match(argument,sm,uuidre);
  if( session->loglevel >= 5 )
    *(session->logfile) << "FIF :: Match Count: " << sm.size() << endl;

  if ( sm.size() > 0 ) {
    string dir1 = sm[1];
    string dir2 = sm[2];
    string uuidfrag = sm[3];
    string uuid = dir1 + dir2 + uuidfrag;
    string fpath = separator + dir1 + separator + dir2 + separator + uuid;

    // check for existence of this file 
    struct stat buffer;   
    if (stat ( (filesystem_prefix + "private/images" + fpath ).c_str(), &buffer) == 0) 
        fpath = "/private/images" + fpath;
    else if (stat ( (filesystem_prefix + "public/images" + fpath ).c_str(), &buffer) == 0) 
        fpath = "/public/images" + fpath;

    string sz = sm[4];
    if ( !sz.empty() )
        fpath = fpath + SIZESEP + sz;

    argument = fpath;

    *(session->logfile) << "FIF dir1: " << dir1 << endl;
    *(session->logfile) << "FIF dir2: " << dir2 << endl;
    *(session->logfile) << "FIF uuid: " << uuid << endl;
    *(session->logfile) << "FIF fpath: " << fpath << endl;
  }
  
  if( session->loglevel >= 5 )
    *(session->logfile) << "FIF :: " << argument << endl;

  // Filter out any ../ to prevent users by-passing any file system prefix
  unsigned int n;
  while( (n=argument.find("../")) < argument.length() ) argument.erase(n,3);

  if( session->loglevel >=1 ){
    if( url.warning().length() > 0 ) 
      *(session->logfile) << "FIF :: " << url.warning() << endl;
    if( session->loglevel >= 5 )
      *(session->logfile) << "FIF :: URL decoding/filtering: " << src << " => " << argument << endl;
  }



  int pos = argument.rfind(separator)+1;
  if ( pos == string::npos )
    pos = 0;
  string originalFileName = argument.substr( pos, argument.length() - pos );
  if( session->loglevel >= 5 )
    *(session->logfile) << "FIF :: original file name " << originalFileName << endl;

  unsigned int max_headers_in_metadata_cache = Environment::getMaxHeadersInMetadataCache();


  // Get our image pattern variable
  string filename_pattern = Environment::getFileNamePattern();

  // Timestamp of cached image
  time_t timestamp = 0;

  int maxInRequest = session->view->getMaxSampleSize();
  int maxPixels = -1;

  if( session->loglevel >= 5 )
    *(session->logfile) << "FIF :: file name " << argument << endl;

  // Put the image setup into a try block as object creation can throw an exception
  try {

    // see if the image filename already has a max resolution marker on it
    // argument += "&blah=blah";
    string::size_type st = argument.find(SIZESEP);
    int filenameMaxPixels = 0;
    if ( st != string::npos ) {
        st += SIZESEP.length();
        string::size_type e = argument.find_first_not_of("0123456789", st);
        string::size_type l = ( e == string::npos ? argument.length()-st : e-st );
        try {
            filenameMaxPixels = stoi(argument.substr(st, l));
        }
        catch(const std::invalid_argument){ 
            throw invalid_argument( "unsupported parameter");
        }

        // trim the filename to remove the size specification before trying to load it
        argument = argument.substr(0,st-SIZESEP.length());

        // if not yet set by the HEADER then set the max sample size based on the extended filename method, 
        // otherwise use the header rather than the size specified in the URL
        if ( maxInRequest == 0 ) {
            maxInRequest = filenameMaxPixels;
            session->view->setMaxSampleSize(maxInRequest);
        }
    }

    int pos = argument.rfind(separator)+1;
    if ( pos == string::npos )
      pos = 0;
    string revisedFileName = argument.substr( pos, argument.length() - pos );
    if ( session->loglevel >= 5 ) {
        *(session->logfile) << "FIF :: file name " << argument << endl;
        *(session->logfile) << "FIF :: revised file name " << revisedFileName << endl;
    }

    // DPB - there is currently a problem with the image headers cache so we're disabling it for now

    // an image cache key is necessary in order to differentiate a regular image load from one 
    // that is being rendered with degraded sampling due to rights restrictions
    ostringstream ss;
    ss << argument << SIZESEP << session->view->getMaxSampleSize();
    string imageCacheKey = ss.str();

    if ( session->loglevel >= 5 )
        *(session->logfile) << "FIF :: Image cache key: " << imageCacheKey << endl;

    IIPImage *test;
    // first thing we do is look in cache for the image
    if ( max_headers_in_metadata_cache > 0 && session->imageCache->find(imageCacheKey) != session->imageCache->end() ) {
      // get the cached image
      test = (*session->imageCache)[ imageCacheKey ];
      timestamp = test->timestamp;       // Record timestamp if we have a cached image
      if ( session->loglevel >= 2 ) 
        *(session->logfile) << "FIF :: Image cache hit. Number of elements: " << session->imageCache->size() << endl;
    }
    else { // new image needs to be created and cached
      if ( max_headers_in_metadata_cache > 0 ) {
        if ( session->imageCache->empty() ) { // cache is empty
          if ( session->loglevel >= 2 && session->imageCache->size() == 0 ) 
            *(session->logfile) << "FIF :: Image cache initialization" << endl;
        }
        else {  // non-empty cache, so check its size and prune the first entry if needs be
          if ( session->loglevel >= 2 ) 
            *(session->logfile) << "FIF :: Image cache miss" << endl;
          if ( session->imageCache->size() >= max_headers_in_metadata_cache ) 
            session->imageCache->erase( session->imageCache->begin() );
        }
      }
      
      test = new IIPImage( argument );
      test->setFileNamePattern( filename_pattern );
      test->setFileSystemPrefix( filesystem_prefix );
      test->setOriginalFileName( originalFileName );
      test->Initialise();

    }

    /*************************************************************************
     Test for different image types - only TIFF and JP2 are supported for now
    **************************************************************************/

    ImageFormat format = test->getImageFormat();

    if ( session->loglevel >= 5 )
        *(session->logfile) << "FIF :: image format: " << format << endl;

    // this section creates the proper subclass based on the format - however, once created, we should just be caching these
    // rather than creating a new object every time - how do we know?
    if ( format == TIF ) {
      if( session->loglevel >= 2 ) 
        *(session->logfile) << "FIF :: TIFF image detected" << endl;
        // use the cached image or create a new one from the IIPImage base if we don't have the image in cache
      (*session->image) = new TPTImage( *test );
    }
#if defined(HAVE_KAKADU) || defined(HAVE_OPENJPEG)
    else if( format == JPEG2000 ) {
      if( session->loglevel >= 2 )
        *(session->logfile) << "FIF :: JPEG2000 image detected" << endl;
#if defined(HAVE_KAKADU)
      *(session->image) = new KakaduImage( *test );
#elif defined(HAVE_OPENJPEG)
      *(session->image) = new OpenJPEGImage( *test );
#endif
    }
#endif
    else 
      throw string( "FIF :: Unsupported image type: " + argument );

    // we're done with the test image now
    delete test;

    /* Disable module loading for now!
    else{

#ifdef ENABLE_DL

      // Check our map list for the requested type
      if( moduleList.empty() ){
	throw string( "Unsupported image type: " + imtype );
      }
      else{

	map<string, string> :: iterator mod_it  = moduleList.find( imtype );

	if( mod_it == moduleList.end() ){
	  throw string( "Unsupported image type: " + imtype );
	}
	else{
	  // Construct our dynamic loading image decoder
	  session->image = new DSOImage( test );
	  (*session->image)->Load( (*mod_it).second );

	  if( session->loglevel >= 2 ){
	    *(session->logfile) << "FIF :: Image type: '" << imtype
	                        << "' requested ... using handler "
				<< (*session->image)->getDescription() << endl;
	  }
	}
      }
#else
      throw string( "Unsupported image type: " + imtype );
#endif
    }
    */

    // Open image and update timestamp - will also update metadata if necessary
    (*session->image)->openImage(session->view->getMaxSampleSize());
    
    // Check timestamp consistency. If cached timestamp is older, update metadata
    if( timestamp>0 && (timestamp < (*session->image)->timestamp) ){
      if ( session->loglevel >= 2 )
	*(session->logfile) << "FIF :: Image timestamp changed: reloading metadata" << endl;
      (*session->image)->loadImageInfo( (*session->image)->currentX, (*session->image)->currentY );
    }

    if( session->loglevel >= 3 )
      *(session->logfile) << "FIF :: Created image" << endl;

    // CHECK EMBEDDED METDATA IN IMAGE FILE 
    std::string input = (*session->image)->getMetadata("xmp");
    if ( !input.empty() ) {
        std::string exifTag = "nga:imgMaxPublicPixels"; // convert to a configuration parameter
        string::size_type start = input.find("<"+exifTag+">");
        string::size_type end = input.find("</"+exifTag+">");

        if ( start != string::npos && end != string::npos ) {
            std:string s = input.substr(start+exifTag.length()+2,end-start-exifTag.length()-2);
            try {

                maxPixels = std::stoi(s);

                /*CASES: 
                    max in HEADER           max in image
                    none                    none                => maxSampling set to 0 
                    0                       none                => maxSampling set to 0 so same situation
                    600                     none                => maxSampling set to 600 so resolutions are clipped even though no restrictions in image per se
                    1600                    none                => maxSampling set to 600 so resolutions are clipped even though no restrictions in image per se
                    none                    640                 => maxSampling set to 0 but image says 640 - so redirect to 640 (max of image) in this case since requested is zero                 ///
                    0                       640                 => same as above - can request 0 but it means same thing as none - cannot request zero pixels anyway
                    600                     640                 => maxSampling set to 600 which is < 640 so let it continue without a redirect
                    1600                    640                 => 1600 > 640 so need redirect to 640                                                                                               ///
                    none                    0                   => cannot display this image at all - should zero restriction simply be ignored? Configuration to define how to handle this case.   ///
                    none                    0                   => should return a 403 when configuration is set to enforce this case                                                               ///
                    0                       0                   => cannot display this image at all - see above
                    600                     0                   => cannot display this image at all - see above
                    1600                    0                   => cannot display this image at all - see above
                */

                // if image sampling size restrictions are defined for this image and no sampling size was requested or 
                // the requested sampling size is larger than the max allowable, don't cache responses to requests for this image
                if ( maxPixels <= 0 || ( maxPixels > 0 && ( maxInRequest <= 0 || maxInRequest > maxPixels ) ) ) {
                    if ( session->loglevel >= 2 ) 
                        *(session->logfile) << "FIF :: SETTING NOT CACHEABLE " << endl;
                    // disable caching of this response in all cases 
                    session->response->setCacheControl("no-cache");
                    session->response->setNotCacheable();
                }

                if ( maxPixels <= 0 ) { 
                    if ( session->view->getEnforceEmbeddedMaxSample() ) {
                        // return 403 forbidden error or instruct caller to do that
                        string header = string( "Status: 403 Forbidden\r\n" ) + 
                                                "Server: iipsrv/" + VERSION + "\r\n" + 
                                                "\r\n";
                        session->out->printf( (const char*) header.c_str() );
                        session->response->setImageSent();
                        session->response->setNotCacheable();
                        return;
                    }
                }
                else if ( ( maxPixels > 0 && maxInRequest <= 0 ) || ( maxPixels < maxInRequest ) )  {
                    if ( session->view->getEnforceEmbeddedMaxSample() ) {
                        string request_uri = session->headers["REQUEST_URI"];

                        if( session->loglevel >= 2 ) {
                            *(session->logfile) << "FIF :: REQUEST_URI: " << request_uri << endl;
                            *(session->logfile) << "FIF :: ARGUMENT: " << argument << endl;
                            *(session->logfile) << "FIF :: ORIG FILENAME: " << originalFileName << endl;
                        }
                        // request_uri.replace(request_uri.find(originalFileName), originalFileName.length(), originalFileName + SIZESEP + to_string(maxPixels));
                        //string newFileName = originalFileName + SIZESEP + to_string(maxPixels);

                        // dpb custom code to replace the uri with one that meshes with the Apache configuration on the front-end
                        //if ( request_uri.find("iiif-private") != string::npos )
                        //    request_uri.replace(request_uri.find(originalFileName), originalFileName.length(), originalFileName + SIZESEP + to_string(maxPixels));
                        //    request_uri = "/iiif/" + newFileName;
                        // request_uri.replace(request.uri.find("iiif-private/public/images/

                        request_uri.replace(request_uri.find(originalFileName), originalFileName.length(), revisedFileName + SIZESEP + to_string(maxPixels));

                        if ( session->loglevel >= 2 ) 
                            *(session->logfile) << "FIF :: REQUEST URI: " << request_uri << endl;
                        string header = string( "Status: 303 See Other\r\n" ) + 
                                                "Location: " + request_uri + "\r\n" + 
                                                "Server: iipsrv/" + VERSION + "\r\n" + 
                                                "\r\n";
                        session->out->printf( (const char*) header.c_str() );
                        session->response->setImageSent();
                        session->response->setNotCacheable();
                        return;
                    }
                }
            }
            catch(const std::invalid_argument){ 
                // conversion to a long failed which is fine - we just ignore this case
            } 
        }
    }


    // Set the timestamp for the reply
    session->response->setLastModified( (*session->image)->getTimestamp() );

    if( session->loglevel >= 2 ) {
      *(session->logfile) << "FIF :: Image dimensions are " << (*session->image)->getImageWidth()
			  << " x " << (*session->image)->getImageHeight() << endl
			  << "FIF :: Image contains " << (*session->image)->channels
			  << " channel" << (((*session->image)->channels>1)?"s":"") << " with "
			  << (*session->image)->bpc << " bit" << (((*session->image)->bpc>1)?"s":"") << " per channel" << endl;
      tm *t = gmtime( &(*session->image)->timestamp );
      char strt[64];
      strftime( strt, 64, "%a, %d %b %Y %H:%M:%S GMT", t );
      *(session->logfile) << "FIF :: Image timestamp: " << strt << endl;
    }

    // Add a copy of this image to cache since we want tiff and tiffbuff to be null as well as
    // to be able to dispose of the session image between sessions;
    //if ( max_headers_in_metadata_cache > 0 )
    //  (*session->imageCache)[imageCacheKey] = new IIPImage( *(*session->image) );

  }
  catch( const file_error& error ){
    // Unavailable file error code is 1 3
    session->response->setError( "1 3", "FIF" );
    *(session->logfile) << "FIF :: ERROR" << endl;
    throw error;
  }


  // Check whether we have had an if_modified_since header. If so, compare to our image timestamp
  if( session->headers.find("HTTP_IF_MODIFIED_SINCE") != session->headers.end() ){
    tm mod_t;
    time_t t;

    strptime( (session->headers)["HTTP_IF_MODIFIED_SINCE"].c_str(), "%a, %d %b %Y %H:%M:%S %Z", &mod_t );

    // Use POSIX cross-platform mktime() function to generate a timestamp.
    // This needs UTC, but to avoid a slow TZ environment reset for each request, we set this once globally in Main.cc
    t = mktime(&mod_t);
    if( (session->loglevel >= 1) && (t == -1) ) *(session->logfile) << "FIF :: Error creating timestamp" << endl;

    if( (*session->image)->timestamp <= t ){
      if( session->loglevel >= 2 ){
	*(session->logfile) << "FIF :: Unmodified content" << endl;
	*(session->logfile) << "FIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
      }
      throw( 304 );
    }
    else{
      if( session->loglevel >= 2 ){
	*(session->logfile) << "FIF :: Content modified since requested time" << endl;
      }
    }
  }

  // Reset our angle values
  session->view->xangle = 0;
  session->view->yangle = 90;

  if( session->loglevel >= 2 ){
    *(session->logfile)	<< "FIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }
}

