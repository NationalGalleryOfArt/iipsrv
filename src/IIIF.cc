/*

    IIIF Request Command Handler Class Member Function

    Copyright (C) 2014-2016 Ruven Pillay

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
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
#include <cmath>
#include <cstdlib>
#include <sstream>
#include "Task.h"
#include "Tokenizer.h"
#include "Transforms.h"
#include "Filters.h"
#include "URL.h"
#include "sys/stat.h"

#if _MSC_VER
#include "../windows/Time.h"
#endif

// Define several IIIF strings
#define IIIF_SYNTAX "IIIF syntax is {identifier}/{region}/{size}/{rotation}/{quality}{.format}"
#define IIIF_PROFILE "http://iiif.io/api/image/2/level1.json"
#define IIIF_CONTEXT "http://iiif.io/api/image/2/context.json"
#define IIIF_PROTOCOL "http://iiif.io/api/image"

using namespace std;

// The request is in the form {identifier}/{region}/{size}/{rotation}/{quality}{.format}
//     eg. filename.tif/full/full/0/native.jpg
// or in the form {identifier}/info.json
//    eg. filename.jp2/info.json

void IIIF::run( Session* session, const string& src )
{

  if ( session->loglevel >= 3 ) *(session->logfile) << "IIIF handler reached" << endl;

  // Time this command
  if ( session->loglevel >= 2 ) command_timer.start();

  // Various string variables
  string suffix, filename, params;

  // First filter and decode our URL
  URL url( src );
  string argument = url.decode();

  if ( session->loglevel >= 1 ){
    if ( url.warning().length() > 0 ) *(session->logfile) << "IIIF :: " << url.warning() << endl;
    if ( session->loglevel >= 5 ){
      *(session->logfile) << "IIIF :: URL decoded to " << argument << endl;
    }
  }

  // if URL points to a file that we have access to, redirect to the info.json service for that file 
    // Get our image pattern variable
  string filecheck = Environment::getFileSystemPrefix() + argument;
  struct stat buffer;
  if ( stat (filecheck.c_str(), &buffer) == 0) {
    // No parameters, so redirect to info request
    string id;
    string host = session->headers["BASE_URL"];
    if ( host.length() > 0 ){
      // id = host + (session->headers["QUERY_STRING"]).substr(5, string::npos);
      id = host + argument; 
    }
    else{
      string request_uri = session->headers["REQUEST_URI"];
      request_uri.erase( request_uri.length() - suffix.length(), string::npos );
      id = "http://" + session->headers["HTTP_HOST"] + request_uri;
    }
    string header = string( "Status: 303 See Other\r\n" )
                    + "Location: " + id + "/info.json\r\n"
                    + "Server: iipsrv/" + VERSION + "\r\n"
                    + "\r\n";
    session->out->printf( (const char*) header.c_str() );
    session->response->setImageSent();
    if ( session->loglevel >= 2 ){
      *(session->logfile) << "IIIF :: Sending HTTP 303 See Other : " << id + "/info.json" << endl;
    }
    return;
  }

  // Check if there is slash in argument and if it is not last / first character, extract identifier and suffix
  size_t lastSlashPos = argument.find_last_of("/");
  if ( lastSlashPos != string::npos ){

    suffix = argument.substr( lastSlashPos + 1, string::npos );

    // If we have an info command, file name must be everything
    if ( suffix.substr(0, 4) == "info" ){
      filename = argument.substr(0, lastSlashPos);
    }
    else{
      size_t positionTmp = lastSlashPos;
      for ( int i = 0; i < 3; i++ ){
        positionTmp = argument.find_last_of("/", positionTmp - 1);
        if ( positionTmp == string::npos ){
          throw invalid_argument( "IIIF: Not enough parameters" );
        }
      }
      filename = argument.substr(0, positionTmp);
      params = argument.substr(positionTmp + 1, string::npos);
    }
  }
  else{
    // No parameters, so redirect to info request
    string id;
    string host = session->headers["BASE_URL"];
    if ( host.length() > 0 ){
      id = host + (session->headers["QUERY_STRING"]).substr(5, string::npos);
    }
    else{
      string request_uri = session->headers["REQUEST_URI"];
      request_uri.erase( request_uri.length() - suffix.length(), string::npos );
      id = "http://" + session->headers["HTTP_HOST"] + request_uri;
    }
    string header = string( "Status: 303 See Other\r\n" )
                    + "Location: " + id + "/info.json\r\n"
                    + "Server: iipsrv/" + VERSION + "\r\n"
                    + "\r\n";
    session->out->printf( (const char*) header.c_str() );
    session->response->setImageSent();
    if ( session->loglevel >= 2 ){
      *(session->logfile) << "IIIF :: Sending HTTP 303 See Other : " << id + "/info.json" << endl;
    }
    return;
  }

  // Check whether requested image exists
  FIF fif;
  fif.run( session, filename );

  // Reload our filename
  filename = (*session->image)->getImagePath();

  // Get the information about image, that can be shown in info.json
  unsigned int requested_width;
  unsigned int requested_height;
  unsigned int width = (*session->image)->getImageWidth();
  unsigned int height = (*session->image)->getImageHeight();
  unsigned tw = (*session->image)->getTileWidth();
  unsigned th = (*session->image)->getTileHeight();
  unsigned numResolutions = (*session->image)->getNumResolutions();

  session->view->setImageSize( width, height );
  session->view->setMaxResolutions( numResolutions );

  // PARSE INPUT PARAMETERS

  // info.json
  if ( suffix == "info.json" ){

    // Our string buffer
    stringstream infoStringStream;

    // Generate our @id - use our BASE_URL environment variable if we are
    //  behind a web server rewrite function
    string id;
    string host = session->headers["BASE_URL"];
    if ( host.length() > 0 ){
      //string query = (session->headers["QUERY_STRING"]);
      // we have the url calculated above already so just use that since there might also be additional params to IIIF in the query string
      string query = argument.substr( 0, argument.length() - suffix.length() - 1 );
      id = host + query;
    }
    else{
      string request_uri = session->headers["REQUEST_URI"];

      string scheme = session->headers["HTTPS"].empty() ? "http://" : "https://";

      if (request_uri.empty()){
        throw invalid_argument( "IIIF: REQUEST_URI was not set in FastCGI request, so the ID parameter cannot be set." );
      }

      request_uri.erase( request_uri.length() - suffix.length() - 1, string::npos );
      id = scheme + session->headers["HTTP_HOST"] + request_uri;
    }

    // Decode and escape any URL-encoded characters from our file name for JSON
*(session->logfile) << "IIIF :: id2 :" << id << endl;
    URL json(id);
    string escapedFilename = json.escape();
*(session->logfile) << "IIIF :: escapedFilename:" << escapedFilename << endl;
    string iiif_id = session->headers["HTTP_X_IIIF_ID"].empty() ? escapedFilename : session->headers["HTTP_X_IIIF_ID"];

    if ( session->loglevel >= 5 ){
      *(session->logfile) << "IIIF :: ID is set to " << iiif_id << endl;
    }

    infoStringStream << "{" << endl
                     << "  \"@context\" : \"" << IIIF_CONTEXT << "\"," << endl
                     << "  \"@id\" : \"" << iiif_id << "\"," << endl
                     << "  \"protocol\" : \"" << IIIF_PROTOCOL << "\"," << endl
                     << "  \"width\" : " << width << "," << endl
                     << "  \"height\" : " << height << "," << endl
                     << "  \"sizes\" : [" << endl
                     << "     { \"width\" : " << (*session->image)->image_widths[numResolutions - 1]
                     << ", \"height\" : " << (*session->image)->image_heights[numResolutions - 1] << " }";

    for ( int i = numResolutions - 2; i > 0; i-- ){
      unsigned int w = (*session->image)->image_widths[i];
      unsigned int h = (*session->image)->image_heights[i];
      unsigned int max = session->view->getMaxSize();
      // Only advertise images below our max size value
      if( (max == 0) || (w < max && h < max) ){
	infoStringStream << "," << endl
			 << "     { \"width\" : " << w << ", \"height\" : " << h << " }";
      }
    }

    infoStringStream << endl << "  ]," << endl
                     << "  \"tiles\" : [" << endl
                     << "     { \"width\" : " << tw << ", \"height\" : " << th
                     << ", \"scaleFactors\" : [ 1"; // Scale 1 is original image

    for ( unsigned int i = 1; i < numResolutions; i++ ){
      infoStringStream << ", " << pow(2.0, (double)i);
    }

    infoStringStream << " ] }" << endl
                     << "  ]," << endl
                     << "  \"profile\" : [" << endl
                     << "     \"" << IIIF_PROFILE << "\"," << endl
                     << "     { \"formats\" : [ \"jpg\" ]," << endl
                     << "       \"qualities\" : [ \"native\",\"color\",\"gray\" ]," << endl
                     << "       \"supports\" : [\"regionByPct\",\"regionSquare\",\"sizeByForcedWh\",\"sizeByWh\",\"sizeAboveFull\",\"rotationBy90s\",\"mirroring\"] }" << endl
                     << "  ]" << endl
                     << "}";

    // Get our Access-Control-Allow-Origin value, if any
    string cors = session->response->getCORS();
    string eof = "\r\n";

    // Now output the info text
    stringstream header;
    header << "Server: iipsrv/" << VERSION << eof
    //     << "Content-Type: application/ld+json" << eof
    // removed ld+json since the specs suggest it's optional and in prezi 3.0 this
    // will move to ld+json but with a profile specification as well
           << "Content-Type: application/json" << eof
           << "Last-Modified: " << (*session->image)->getTimestamp() << eof
           << session->response->getCacheControl() << eof;

    if ( !cors.empty() ) header << cors << eof;
    header << eof << infoStringStream.str();

    session->out->printf( (const char*) header.str().c_str() );
    session->response->setImageSent();

    return;

  }
  // Parse image request - any other than info requests are considered image requests
  else{

    // IIIF requests are / separated with no CGI style '&' separators
    Tokenizer izer( params, "/" );

    // Keep track of the number of parameters than have been given
    int numOfTokens = 0;

    // Region Parameter: { "full"; "x,y,w,h"; "pct:x,y,w,h" }
    if ( izer.hasMoreTokens() ){

      // Our region parameters
      float region[4];

      // Get our region string and convert to lower case if necessary
      string regionString = izer.nextToken();
      transform( regionString.begin(), regionString.end(), regionString.begin(), ::tolower );

      // Full export request
      if ( regionString == "full" ){
        region[0] = 0.0;
        region[1] = 0.0;
        region[2] = 1.0;
        region[3] = 1.0;
      }
      // Square region export using centered crop
      else if (regionString == "square" ){
        if ( height > width ){
	  float h = (float)width/(float)height;
	  session->view->setViewTop( (1-h)/2.0 );
	  session->view->setViewHeight( h );
        }
	else if ( width > height ){
	  float w = (float)height/(float)width;
	  session->view->setViewLeft( (1-w)/2.0 );
	  session->view->setViewWidth( w );
        }
	// No need for default else clause if image is already square
      }

      // Region export request
      else{

        // Check for pct (%) and strip it from the beginning
        bool isPCT = false;
        if ( regionString.substr(0, 4) == "pct:" ){
          isPCT = true;
          // Strip this from our string
          regionString.erase( 0, 4 );
        }

        // Extract x,y,w,h tokenizing on ","
        Tokenizer regionIzer(regionString, ",");
        int n = 0;

        // Extract our region values
        while ( regionIzer.hasMoreTokens() && n < 4 ){
          region[n++] = atof( regionIzer.nextToken().c_str() );
        }

        // Define our denominators as our session view expects a ratio, not pixel values
        //float wd = (float)width;
        //float hd = (float)height;

        int x1check = 0; // upper left corner of region
        int y1check = 0; 
        int x2check = 0; // lower right corner of region
        int y2check = 0;

        // recalculate percentage after performing integer math on the original size first since we have to do some checks
        // to make sure everything is within bounds, potentially perform some clipping, etc.  Only after that is is safe to
        // recompute the percentages to use on subsequent resolutions
        if ( isPCT ){
          //wd = 100.0;
          //hd = 100.0;
            x1check = (region[0] / 100.0) * width;  
            y1check = (region[1] / 100.0) * height; 
            x2check = x1check + (region[2] / 100.0) * width;  
            y2check = y1check + (region[3] / 100.0) * height; 
        }
        else {
            x1check = region[0];
            y1check = region[1];
            x2check = x1check + region[2];
            y2check = y1check + region[3];
        }

logfile << "width: " << width << endl;
logfile << "height: " << height << endl;

logfile << "region0: " << region[0] << endl;
logfile << "region1: " << region[1] << endl;
logfile << "region2: " << region[2] << endl;
logfile << "region3: " << region[3] << endl;

logfile << "x1ca: " << x1check << endl;
logfile << "y1ca: " << y1check << endl;
logfile << "x2ca: " << x2check << endl;
logfile << "y2ca: " << y2check << endl;

        // now, if the start of the region is beyond the max extends or the end is before the max extents, it doesn't overlap at all with the image
        // so we need to throw an error
        if ( (x1check >= (int) width || x2check <= 0) || (y1check > (int) height || y2check <= 0) )
            throw invalid_argument( "invalid region 1: the specified region does not intersect with the geometry of the image" );
        if ( (x1check >= x2check || y1check >= y2check) )
            throw invalid_argument( "invalid region 2: the specified region does not intersect with the geometry of the image" );

        // otherwise, we need to clamp the values so they are all valid w.r.t. the original image
        x1check = CLAMP<int>(x1check, 0, width-1);
        y1check = CLAMP<int>(y1check, 0, height-1);
        x2check = CLAMP<int>(x2check, 0, width);
        y2check = CLAMP<int>(y2check, 0, height);

        // + 1 because we want the full width of the region rather the difference between the first and last column or row
        int widthinpix = x2check-x1check;
        //if (widthinpix <= 0)
        //    widthinpix = 1;
        int heightinpix = y2check-y1check;
        //if (heightinpix <= 0)
        //    heightinpix = 1;

        //if ( (x1check >= x2check ) || (y1check >= y2check ) )
        //    throw invalid_argument( "invalid region: the resulting region on the target image has zero width or zero height" );

logfile << "x1cb: " << x1check << endl;
logfile << "y1cb: " << y1check << endl;
logfile << "x2cb: " << x2check << endl;
logfile << "y2cb: " << y2check << endl;

logfile << "widthinpix: " << widthinpix << endl;
logfile << "heightinpix: " << heightinpix << endl;

        // and finally, we set this now guaranteed valid region to a proportion of the original image
        session->view->setViewLeft(     (float) x1check         / (float) width );
        session->view->setViewTop(      (float) y1check         / (float) height );
        session->view->setViewWidth(    (float) widthinpix      / (float) width );  
        session->view->setViewHeight(   (float) heightinpix     / (float) height ); 

        /*session->view->setViewLeft( region[0] / wd );
        session->view->setViewTop( region[1] / hd );
        session->view->setViewWidth( region[2] / wd );
        session->view->setViewHeight( region[3] / hd );*/

logfile << "x1c: " << x1check << endl;
logfile << "y1c: " << y1check << endl;
logfile << "x2c: " << x2check << endl;
logfile << "y2c: " << y2check << endl;

logfile << "vl: " << session->view->getViewLeft() << endl;
logfile << "vt: " << session->view->getViewTop() << endl;
logfile << "vw: " << session->view->getViewWidth() << endl;
logfile << "vh: " << session->view->getViewHeight() << endl;

        // Incorrect region request
        if ( region[2] <= 0.0 || region[3] <= 0.0 || regionIzer.hasMoreTokens() || n < 4 ){
          throw invalid_argument( "IIIF: incorrect region format: " + regionString );
        }

      } // end of else - end of parsing x,y,w,h

      numOfTokens++;

      if ( session->loglevel > 4 ){
        *(session->logfile) << "IIIF :: Requested Region: x:" << region[0] << ", y:" << region[1]
                            << ", w:" << region[2] << ", h:" << region[3] << endl;
      }

    }

    // Size Parameter: { "full"; "w,"; ",h"; "pct:n"; "w,h"; "!w,h" }
    if ( izer.hasMoreTokens() ){

      string sizeString = izer.nextToken();
      transform( sizeString.begin(), sizeString.end(), sizeString.begin(), ::tolower );

      // Calculate the width and height of our region
      requested_width = session->view->getViewWidth();
      requested_height = session->view->getViewHeight();
      float ratio = (float)requested_width / (float)requested_height;
      unsigned int max_size = session->view->getMaxSize();

      // "full" request
      if ( sizeString == "full" ){
        // No need to do anything
      }

      // "pct:n" request
      else if ( sizeString.substr(0, 4) == "pct:" ){

        float scale;
        istringstream i( sizeString.substr( sizeString.find_first_of(":") + 1, string::npos ) );
        if ( !(i >> scale) ) throw invalid_argument( "invalid size" );

        requested_width = round( requested_width * scale / 100.0 );
        requested_height = round( requested_height * scale / 100.0 );
      }

      // "w,h", "w,", ",h", "!w,h" requests
      else{

        // !w,h request - remove !, remember it and continue as if w,h request
        if ( sizeString.substr(0, 1) == "!" ) sizeString.erase(0, 1);
        // Otherwise tell our view to break aspect ratio
        else session->view->maintain_aspect = false;

        size_t pos = sizeString.find_first_of(",");

        // If no comma, size is invalid
        if ( pos == string::npos ){
          throw invalid_argument( "invalid size: no comma found" );
        }

        // If comma is at the beginning, we have a ",height" request
        else if ( pos == 0 ){
          istringstream i( sizeString.substr( 1, string::npos ) );
          if ( !(i >> requested_height) ) throw invalid_argument( "invalid height" );
	  requested_width = round( (float)requested_height * ratio );
	  // Maintain aspect ratio in this case
 	  session->view->maintain_aspect = true;
        }

        // If comma is not at the beginning, we must have a "width,height" or "width," request
        // Test first for the "width," request
        else if ( pos == sizeString.length() - 1 ){
          istringstream i( sizeString.substr( 0, string::npos - 1 ) );
          if ( !(i >> requested_width ) ) throw invalid_argument( "invalid width" );
	  requested_height = round( (float)requested_width / ratio );
	  // Maintain aspect ratio in this case
 	  session->view->maintain_aspect = true;
        }

        // Remaining case is "width,height"
        else{
          istringstream i( sizeString.substr( 0, pos ) );
          if ( !(i >> requested_width) ) throw invalid_argument( "invalid width" );
          i.clear();
          i.str( sizeString.substr( pos + 1, string::npos ) );
          if ( !(i >> requested_height) ) throw invalid_argument( "invalid height" );
        }
      }

      if ( requested_width == 0 || requested_height == 0 ){
        throw invalid_argument( "IIIF: invalid size" );
      }

      // Limit our requested size to the maximum allowable size if necessary
      if( requested_width > max_size || requested_height > max_size ){
	if( ratio > 1.0 ){
	  requested_width = max_size;
	  requested_height = session->view->maintain_aspect ? round(max_size*ratio) : max_size;
	}
	else{
	  requested_height = max_size;
	  requested_width = session->view->maintain_aspect ? round(max_size/ratio) : max_size;
	}
      }

      session->view->setRequestWidth( requested_width );
      session->view->setRequestHeight( requested_height );

      numOfTokens++;

      if ( session->loglevel >= 4 ){
        *(session->logfile) << "IIIF :: Requested Size: " << requested_width << "x" << requested_height << endl;
      }

    }

    // Rotation Parameter
    if ( izer.hasMoreTokens() ){

      string rotationString = izer.nextToken();

      // Flip requests (IIIF 2.0 API)
      if ( rotationString.substr(0, 1) == "!" ){
        session->view->flip = 1;
        rotationString.erase(0, 1);
      }

      // Convert our string to a float
      float rotation = 0;
      istringstream i( rotationString );
      if ( !(i >> rotation) ) throw invalid_argument( "IIIF: invalid rotation" );


      // Check if converted value is supported
      if (!( rotation == 0 || rotation == 90 || rotation == 180 || rotation == 270 || rotation == 360 )){
        throw invalid_argument( "IIIF: currently implemented rotation angles are 0, 90, 180 and 270 degrees" );
      }

      // Set rotation - watch for a '!180' request, which is simply a vertical flip
      if ( rotation == 180 && session->view->flip == 1 ) session->view->flip = 2;
      else session->view->setRotation( rotation );

      numOfTokens++;

      if ( session->loglevel >= 4 ){
        *(session->logfile) << "IIIF :: Requested Rotation: " << rotation << " degrees";
        if ( session->view->flip != 0 ) *(session->logfile) << " with horizontal flip";
        *(session->logfile) << endl;
      }

    }

    // Quality and Format Parameters
    if ( izer.hasMoreTokens() ){
      string format = "jpg";
      string quality = izer.nextToken();
      transform( quality.begin(), quality.end(), quality.begin(), ::tolower );

      size_t pos = quality.find_last_of(".");

      // Format - if dot is not present, we use default and currently only supported format - JPEG
      if ( pos != string::npos ){
        format = quality.substr( pos + 1, string::npos );
        quality.erase( pos, string::npos );
#ifdef HAVE_PNG
        if ( format != "jpg" && format != "png" ) {
          throw invalid_argument( "IIIF :: Only JPEG and PNG output supported" );
#else
        if ( format != "jpg" ) {
          throw invalid_argument( "IIIF :: Only JPEG output supported" );
#endif
        }
#ifdef HAVE_PNG
        if ( format == "png" )
            session->outputCompressor=session->png;
#endif
      }

      // Quality
      if ( quality == "native" || quality == "color" || quality == "default" ){
        // Do nothing
      }
      else if ( quality == "grey" || quality == "gray" || quality == "grayscale" || quality == "greyscale" ){
        session->view->colourspace = GREYSCALE;
      }
      else if ( quality == "bitonal" ){
        session->view->colourspace = GREYSCALE;
        session->view->bitonal = true;
      }
      else{
        // IIIF image spec 2.1 says the image server SHOULD return a 400 error but if we want to support JPG and PNG
        // lossy quality we might consider enabling a syntax that allows clients to fine tune the quality
        // by manipulating the URL as the QLT factor of the IIP Protocol currently allows, e.g. default.90.jpg for JPEG 
        // or default.PNG_FILTER_AVG.png for PNG, etc.  In consulting with the IIIF spec authors, this has been discussed
        // but there were insufficient use cases to justify the spec taking it up right now.
        // see https://github.com/IIIF/iiif.io/issues/294 for the history - @beaudet
        throw invalid_argument( "unsupported quality parameter - must be one of native, color,  grey, or bitonal (currently unsupported in IIP)" );
      }

      numOfTokens++;

      if ( session->loglevel >= 4 ){
        *(session->logfile) << "IIIF :: Requested Quality: " << quality << " with format: " << format << endl;
      }
    }

    // Too many parameters
    if ( izer.hasMoreTokens() ){
      throw invalid_argument( "IIIF: Query has too many parameters. " IIIF_SYNTAX );
    }

    // Not enough parameters
    if ( numOfTokens < 4 ){
      throw invalid_argument( "IIIF: Query has too few parameters. " IIIF_SYNTAX );
    }

  }
  // End of parsing input parameters

  // Write info about request to log
  if ( session->loglevel >= 3 ){
    if ( suffix == "info.json" ){
      *(session->logfile) << "IIIF :: " << suffix << " request for " << (*session->image)->getImagePath() << endl;
    }
    else{
      *(session->logfile) << "IIIF :: image request for " << (*session->image)->getImagePath()
                          << " with arguments: region: " << session->view->getViewLeft() << "," << session->view->getViewTop() << ","
                          << session->view->getViewWidth() << "," << session->view->getViewHeight()
                          << "; size: " << requested_width << "x" << requested_height
                          << "; rotation: " << session->view->getRotation()
                          << "; mirroring: " << session->view->flip
                          << endl;
    }
  }

  // Get most suitable resolution and recalculate width and height of region in this resolution
  int requested_res = session->view->getResolution(1.0);

  unsigned int im_width = (*session->image)->image_widths[numResolutions - requested_res - 1];
  unsigned int im_height = (*session->image)->image_heights[numResolutions - requested_res - 1];

  unsigned int view_left, view_top;

  if ( session->view->viewPortSet() ){
    // Set the absolute viewport size and extract the co-ordinates
    view_left = session->view->getViewLeft();
    view_top = session->view->getViewTop();
  }
  else{
    view_left = 0;
    view_top = 0;
  }

  // now, for a little sanity checking per the Image API specs - if 
  // calculate the extents where x1,y1 is upper left corner and x2,y2 is lower right corner
  // and if the region is completely off the image and doesn't intersect at all with it, cropping won't help
  // so we return a server 400 (bad request) response
  int x1 = view_left;
  int y1 = view_top;
  int x2 = view_left + session->view->getViewWidth();
  int y2 = view_top  + session->view->getViewHeight();

  logfile << "x1: " << x1 << endl;
  logfile << "y1: " << y1 << endl;
  logfile << "x2: " << x2 << endl;
  logfile << "y2: " << y2 << endl;
  logfile << "im_width: " << im_width << endl;
  logfile << "im_height: " << im_height << endl;

  // if the left point is beyond the image on the right side or vice versa as well as similar check for y dimension
  // then the region does not intersect with the image at all so cropping it won't help (and would in fact be an invalid crop)
  if ( (x1 > im_width || x2 < 0) || (y1 > im_height || y2 < 0) )
      throw invalid_argument( "invalid region: the specified region does not intersect with the geometry of the image" );
  if ( (x1 > x2 || y1 > y2) )
      throw invalid_argument( "invalid region: the specified region does not intersect with the geometry of the image" );

  // Determine whether this is a tile request which coincides with our tile boundaries
  if ( ( session->view->maintain_aspect && (requested_res > 0) &&
         (requested_width == tw) && (requested_height == th) &&
         (view_left % tw == 0) && (view_top % th == 0) &&
         (session->view->getViewWidth() % tw == 0) && (session->view->getViewHeight() % th == 0) &&
         (session->view->getViewWidth() < im_width) && (session->view->getViewHeight() < im_height) )
       ||
       ( session->view->maintain_aspect && (requested_res == 0) &&
         (requested_width == im_width) && (requested_height == im_height) )
     ){

    // Get the width and height for last row and column tiles
    unsigned int rem_x = im_width % tw;

    // Calculate the number of tiles in each direction
    unsigned int ntlx = (im_width / tw) + (rem_x == 0 ? 0 : 1);

    // Calculate tile index
    unsigned int i = view_left / tw;
    unsigned int j = view_top / th;
    unsigned int tile = (j * ntlx) + i;

    // Simply pass this on to our JTL send command
    JTL jtl;
    jtl.send( session, requested_res, tile );

  }
  else{
    // Otherwise do a CVT style region request
    CVT cvt;
    cvt.send( session );
  }

  // Total IIIF response time
  if ( session->loglevel >= 2 ){
    *(session->logfile) << "IIIF :: Total command time " << command_timer.getTime() << " microseconds" << endl;
  }

}
