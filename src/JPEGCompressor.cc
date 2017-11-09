/*  JPEG class wrapper to ijg jpeg library

    Copyright (C) 2000-2015 Ruven Pillay.

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



#include "JPEGCompressor.h"
#include "jpeglib.h"

#define SIZEOF(object)  ((size_t) sizeof(object))
#define MEMCOPY(dest,src,size)  memcpy((void *)(dest), (const void *)(src), (size_t)(size))

#define MX 32768

LOCAL(int)
text_getc (FILE * file)
/* Read next char, skipping over any comments (# to end of line) */
/* A comment/newline sequence is returned as a newline */
{
  register int ch;

  ch = getc(file);
  if (ch == '#') {
    do {
      ch = getc(file);
    } while (ch != '\n' && ch != EOF);
  }
  return ch;
}


LOCAL(boolean)
read_text_integer (FILE * file, long * result, int * termchar)
/* Read an unsigned decimal integer from a file, store it in result */
/* Reads one trailing character after the integer; returns it in termchar */
{
  register int ch;
  register long val;

  /* Skip any leading whitespace, detect EOF */
  do {
    ch = text_getc(file);
    if (ch == EOF) {
      *termchar = ch;
      return FALSE;
    }
  } while (isspace(ch));

  if (! isdigit(ch)) {
    *termchar = ch;
    return FALSE;
  }

  val = ch - '0';
  while ((ch = text_getc(file)) != EOF) {
    if (! isdigit(ch))
      break;
    val *= 10;
    val += ch - '0';
  }
  *result = val;
  *termchar = ch;
  return TRUE;
}

LOCAL(boolean)
read_scan_integer (FILE * file, long * result, int * termchar)
/* Variant of read_text_integer that always looks for a non-space termchar;
 * this simplifies parsing of punctuation in scan scripts.
 */
{
  register int ch;

  if (! read_text_integer(file, result, termchar))
    return FALSE;
  ch = *termchar;
  while (ch != EOF && isspace(ch))
    ch = text_getc(file);
  if (isdigit(ch)) {            /* oops, put it back */
    if (ungetc(ch, file) == EOF)
      return FALSE;
    ch = ' ';
  } else {
    /* Any separators other than ';' and ':' are ignored;
     * this allows user to insert commas, etc, if desired.
     */
    if (ch != EOF && ch != ';' && ch != ':')
      ch = ' ';
  }
  *termchar = ch;
  return TRUE;
}


GLOBAL(boolean)
read_scan_script (j_compress_ptr cinfo, string filename)
/* Read a scan script from the specified text file.
 * Each entry in the file defines one scan to be emitted.
 * Entries are separated by semicolons ';'.
 * An entry contains one to four component indexes,
 * optionally followed by a colon ':' and four progressive-JPEG parameters.
 * The component indexes denote which component(s) are to be transmitted
 * in the current scan.  The first component has index 0.
 * Sequential JPEG is used if the progressive-JPEG parameters are omitted.
 * The file is free format text: any whitespace may appear between numbers
 * and the ':' and ';' punctuation marks.  Also, other punctuation (such
 * as commas or dashes) can be placed between numbers if desired.
 * Comments preceded by '#' may be included in the file.
 * Note: we do very little validity checking here;
 * jcmaster.c will validate the script parameters.
 */
{
  FILE * fp;
  int scanno, ncomps, termchar;
  long val;
  jpeg_scan_info * scanptr;
#define MAX_SCANS  100          /* quite arbitrary limit */
  jpeg_scan_info scans[MAX_SCANS];

  if ((fp = fopen(filename.c_str(), "r")) == NULL) {
    fprintf(stderr, "Can't open scan definition file %s\n", filename.c_str());
    return FALSE;
  }
  scanptr = scans;
  scanno = 0;

  while (read_scan_integer(fp, &val, &termchar)) {
    if (scanno >= MAX_SCANS) {
      fprintf(stderr, "Too many scans defined in file %s\n", filename.c_str());
      fclose(fp);
      return FALSE;
    }
    scanptr->component_index[0] = (int) val;
    ncomps = 1;
    while (termchar == ' ') {
      if (ncomps >= MAX_COMPS_IN_SCAN) {
        fprintf(stderr, "Too many components in one scan in file %s\n", filename.c_str());
        fclose(fp);
        return FALSE;
      }
      if (! read_scan_integer(fp, &val, &termchar))
        goto bogus;
      scanptr->component_index[ncomps] = (int) val;
      ncomps++;
    }
    scanptr->comps_in_scan = ncomps;
    if (termchar == ':') {
      if (! read_scan_integer(fp, &val, &termchar) || termchar != ' ')
        goto bogus;
      scanptr->Ss = (int) val;
      if (! read_scan_integer(fp, &val, &termchar) || termchar != ' ')
        goto bogus;
      scanptr->Se = (int) val;
      if (! read_scan_integer(fp, &val, &termchar) || termchar != ' ')
        goto bogus;
      scanptr->Ah = (int) val;
      if (! read_scan_integer(fp, &val, &termchar))
        goto bogus;
      scanptr->Al = (int) val;
    } else {
      /* set non-progressive parameters */
      scanptr->Ss = 0;
      scanptr->Se = DCTSIZE2-1;
      scanptr->Ah = 0;
      scanptr->Al = 0;
    }
    if (termchar != ';' && termchar != EOF) {
bogus:
      fprintf(stderr, "Invalid scan entry format in file %s\n", filename.c_str());
      fclose(fp);
      return FALSE;
    }
    scanptr++, scanno++;
  }

  if (termchar != EOF) {
    fprintf(stderr, "Non-numeric data in file %s\n", filename.c_str());
    fclose(fp);
    return FALSE;
  }
  if (scanno > 0) {
    /* Stash completed scan list in cinfo structure.
     * NOTE: for cjpeg's use, JPOOL_IMAGE is the right lifetime for this data,
     * but if you want to compress multiple images you'd want JPOOL_PERMANENT.
     */
    scanptr = (jpeg_scan_info *)
      (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_IMAGE,
                                  scanno * SIZEOF(jpeg_scan_info));
    MEMCOPY(scanptr, scans, scanno * SIZEOF(jpeg_scan_info));
    cinfo->scan_info = scanptr;
    cinfo->num_scans = scanno;
  }

  fclose(fp);
  return TRUE;
}


/* My version of the JPEG error_exit function. We want to pass control back
   to the program, so simply throw an exception
*/

METHODDEF(void) iip_error_exit( j_common_ptr cinfo )
{
  char buffer[ JMSG_LENGTH_MAX ];

  /* Create the message
   */
  (*cinfo->err->format_message) ( cinfo, buffer );

  /* Let the memory manager delete any temp files before we die
   */
  jpeg_destroy( cinfo );

  /* throw an exception rather than print out a message and exit
   */
  throw string( buffer );
}



extern "C" {

  void setup_error_functions( jpeg_compress_struct *a ){
    a->err->error_exit = iip_error_exit; 
  }
}




/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

METHODDEF(void)
iip_init_destination (j_compress_ptr cinfo)
{
  size_t mx;
  iip_dest_ptr dest = (iip_dest_ptr) cinfo->dest;

  /* If we have set the strip height, we must be doing a buffer to buffer
     compression, so only allocate enough for this strip. Otherwise allocate
     memory for the whole image
  */
  if( dest->strip_height > 0 ){
    mx = cinfo->image_width * dest->strip_height * cinfo->input_components;
  }
  else{
    mx = cinfo->image_width * cinfo->image_height * cinfo->input_components;
  }

  /* Add some extra because when we have very small tiles, the JPEG data
     including header can end up being larger than the original raw
     data size, especially at high quality factors!
  */
  mx += MX;

  /* Allocate the output buffer --- it will be released when done with image   
  dest->buffer = (JOCTET *)
    (*cinfo->mem->alloc_small) ( (j_common_ptr) cinfo, JPOOL_IMAGE,
				 mx * sizeof(JOCTET) );
  */

  // In fact just allocate with new
  dest->buffer = new JOCTET[mx];
  dest->size = mx;

  // Set compressor pointers for library
  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = mx;
}




METHODDEF(boolean)
iip_empty_output_buffer( j_compress_ptr cinfo )
{
  iip_dest_ptr dest = (iip_dest_ptr) cinfo->dest;
  size_t datacount = dest->size;

  // Copy the JPEG data to our output tile buffer
  if( datacount > 0 ){
    if( datacount > cinfo->image_width*dest->strip_height*cinfo->input_components + MX ){
      datacount = cinfo->image_width*dest->strip_height*cinfo->input_components + MX;
    }
    memcpy( dest->source, dest->buffer, datacount );
  }

  // Reset the pointer to the beginning of the buffer
  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = datacount;

  return TRUE;
}




/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

void iip_term_destination( j_compress_ptr cinfo )
{
  iip_dest_ptr dest = (iip_dest_ptr) cinfo->dest;
  size_t datacount = dest->size - dest->pub.free_in_buffer;

  // Copy the JPEG data to our output tile buffer
  if( datacount > 0 ){
    memcpy( dest->source, dest->buffer, datacount );
  }

  dest->size = datacount;

  delete[] dest->buffer;
}




void JPEGCompressor::InitCompression( const RawTile& rawtile, unsigned int strip_height, unsigned long icc_profile_len, unsigned char *icc_profile_buf ) throw (string)
{

  // Do some initialisation
  dest = &dest_mgr;

  // Set up the correct width and height for this particular tile
  width = rawtile.width;
  height = rawtile.height;
  channels = rawtile.channels;


  // Make sure we only try to compress images with 1 or 3 channels
  if( ! ( (channels==1) || (channels==3) )  ){
    throw string( "JPEGCompressor: JPEG can only handle images of either 1 or 3 channels" );
  }

  // JPEG can only handle 8 bit data
  if( rawtile.bpc != 8 ) throw string( "JPEGCompressor: JPEG can only handle 8 bit images" );


  // We set up the normal JPEG error routines, then override error_exit.
  cinfo.err = jpeg_std_error( &jerr );


  // Overide the error_exit function with our own.
  // Hmmm, we have to do this assignment in C due to the strong type checking of C++
  //  or something like that. So, we use an extern "C" function declared at the top
  //  of this file and pass our arguments through this. I'm sure there's a better
  //  way of doing this, but this seems to work :/

  //   cinfo.err.error_exit = iip_error_exit;
  setup_error_functions( &cinfo );

  jpeg_create_compress( &cinfo );


  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same file without re-executing jpeg_stdio_dest.
   * This makes it dangerous to use this manager and a different destination
   * manager serially with the same JPEG object, because their private object
   * sizes may be different.  Caveat programmer.
   */
  if( !cinfo.dest ){
    // first time for this JPEG object?
    cinfo.dest = ( struct jpeg_destination_mgr* )
      ( *cinfo.mem->alloc_small )
      ( (j_common_ptr) &cinfo, JPOOL_PERMANENT, sizeof( iip_destination_mgr ) );
  }


  dest = (iip_dest_ptr) cinfo.dest;
  dest->pub.init_destination = iip_init_destination;
  //dest->pub.empty_output_buffer = iip_empty_output_buffer;
  dest->pub.term_destination = iip_term_destination;
  dest->strip_height = strip_height;

  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = channels;
  cinfo.in_color_space = ( channels == 3 ? JCS_RGB : JCS_GRAYSCALE );
  jpeg_set_defaults( &cinfo );

  // Set compression point quality (highest, but possibly slower depending
  //  on hardware) - must do this after we've set the defaults!
  cinfo.dct_method = JDCT_FASTEST;

  jpeg_set_quality( &cinfo, Q, TRUE );

  /*************************************************************************
   for future support of JPEGs (progresive) and PNGs (interlaced) - @beaudet
   jpeg_simple_progression( &cinfo );
   HOWEVER: it appears the way that IIP compresses strips (sequentially) might 
   simply be at odds with the way progressive JPEG compression works, at least
   for CVTs... so, it would seem we'd have to junk this approach and compress the
   entire image using Compress (which I'm guessing would work) but then it would be
   slower time to first byte I think - so, ultimately, the best solution might be
   for a memcache helper to visit the contents of the cache with the sole purpose
   of turning those cached images into progressive JPEGs... or just relying on a
   CDN to do it instead.  All this said, I still think we can serve tiles using
   progressive JPEGs, so I'm going to apply simple progression to the Compress
   function and test that
   SOME TEST CODE BELOW THAT MIGHT ONE DAY BE HELPFUL
   // enable progressive jpeg support but only if the requested width and height
   // size
   //  if ( width <= 1024 && height <= 1024 )
   //string fname = "scanscript.txt";
   //if (! read_scan_script(&cinfo, fname ) )
   //  logfile << "ERROR: Could not ready scanscript.txt" << endl;
   // jpeg_simple_progression( &cinfo );
  *************************************************************************/

  jpeg_start_compress( &cinfo, TRUE );

  // Copy the JPEG header data to our output tile buffer
  size_t datacount = dest->size - dest->pub.free_in_buffer;
  header_size = datacount;
  if( datacount > 0 ){
    memcpy( header, dest->buffer, datacount );
  }

  // Reset the pointers
  size_t mx = cinfo.image_width * strip_height * cinfo.input_components;
  dest->size = mx + MX;
  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = mx + MX;

  // Add an identifying comment
  jpeg_write_marker( &cinfo, JPEG_COM, (const JOCTET*) "Generated by IIPImage", 21 );

  // embed icc profile if one is supplied 
  if ( icc_profile_buf != NULL )
    write_icc_profile( &cinfo, icc_profile_buf, icc_profile_len );
}



/*
  We use a separate tile_height from the predefined strip_height because
  the tile height for the final row can be different
 */
unsigned int JPEGCompressor::CompressStrip( unsigned char* input, unsigned char* output, unsigned long outputlen, unsigned int tile_height ) throw (string)
{
  JSAMPROW row[1];
  int row_stride = width * channels;
  dest->source = output;

  while( cinfo.next_scanline < tile_height ) {
    row[0] = &input[ cinfo.next_scanline * row_stride ];
    jpeg_write_scanlines( &cinfo, row, 1 );
  }

  // Copy the JPEG data to our output tile buffer
  size_t datacount = dest->size - dest->pub.free_in_buffer;
  if( datacount > 0 ){
    // Be careful not to overun our buffer
    if( datacount > tile_height*width*channels + MX ) datacount = tile_height*width*channels + MX;
    memcpy( output, dest->buffer, datacount );
  }

  // Set compressor pointers for library
  size_t mx = cinfo.image_width * dest->strip_height * cinfo.input_components;
  dest->pub.next_output_byte = dest->buffer;
  dest->pub.free_in_buffer = mx + MX;
  cinfo.next_scanline = 0;
  dest->size = mx + MX;

  return datacount;
}




unsigned int JPEGCompressor::Finish( unsigned char* output, unsigned long output_len ) throw (string)
{
  dest->source = output;

  // Tidy up and de-allocate memory
  dest->pub.next_output_byte = dest->buffer;
  cinfo.next_scanline = dest->strip_height;
  jpeg_finish_compress( &cinfo );

  size_t datacount = dest->size;

  jpeg_destroy_compress( &cinfo );

  return datacount;
}




int JPEGCompressor::Compress( RawTile& rawtile, unsigned long icc_profile_len, unsigned char *icc_profile_buf ) throw (string)
{

  // Do some initialisation
  data = (unsigned char*) rawtile.data;
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  iip_destination_mgr dest_mgr;
  iip_dest_ptr dest = &dest_mgr;


  // Set up the correct width and height for this particular tile
  width = rawtile.width;
  height = rawtile.height;
  channels = rawtile.channels;


  // Make sure we only try to compress images with 1 or 3 channels
  if( ! ( (channels==1) || (channels==3) ) ){
    throw string( "JPEGCompressor: JPEG can only handle images of either 1 or 3 channels" );
  }

  // JPEG can only handle 8 bit data
  if( rawtile.bpc != 8 ) throw string( "JPEGCompressor: JPEG can only handle 8 bit images" );

  // We set up the normal JPEG error routines, then override error_exit.
  cinfo.err = jpeg_std_error( &jerr );


  // Overide the error_exit function with our own.
  // Hmmm, we have to do this assignment in C due to the strong type checking of C++
  //  or something like that. So, we use an extern "C" function declared at the top
  //  of this file and pass our arguments through this. I'm sure there's a better
  //  way of doing this, but this seems to work :/

  //   cinfo.err.error_exit = iip_error_exit;
  setup_error_functions( &cinfo );

  jpeg_create_compress( &cinfo );


  /* The destination object is made permanent so that multiple JPEG images
   * can be written to the same file without re-executing jpeg_stdio_dest.
   * This makes it dangerous to use this manager and a different destination
   * manager serially with the same JPEG object, because their private object
   * sizes may be different.  Caveat programmer.
   */

  if( !cinfo.dest ){

    // first time for this JPEG object?
    cinfo.dest = ( struct jpeg_destination_mgr* )
      ( *cinfo.mem->alloc_small )
      ( (j_common_ptr) &cinfo, JPOOL_PERMANENT, sizeof( iip_destination_mgr ) );
  }


  dest = (iip_dest_ptr) cinfo.dest;
  dest->pub.init_destination = iip_init_destination;
  dest->pub.empty_output_buffer = iip_empty_output_buffer;
  dest->pub.term_destination = iip_term_destination;
  dest->strip_height = 0;

  // Allocate memory for our destination
  dest->source = new unsigned char[width*height*channels + MX]; // Add some extra buffering

  // Set floating point quality (highest, but possibly slower depending
  //  on hardware)
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = channels;
  cinfo.in_color_space = ( channels == 3 ? JCS_RGB : JCS_GRAYSCALE );
  jpeg_set_defaults( &cinfo );

  // Set compression quality (highest, but possibly slower depending
  //  on hardware) - must do this after we've set the defaults!
  cinfo.dct_method = JDCT_FASTEST;

  jpeg_set_quality( &cinfo, Q, TRUE );

  /*************************************************************************
   for future support of JPEGs (progresive) and PNGs (interlaced) - @beaudet
  jpeg_simple_progression( &cinfo );
  HOWEVER: While this works for tiles, it appears to have a performance impact
  so it might not be worth it, at least not when serving up the initial tiles
  ; it might be better to address this by adjusting the cache in memcache
  with a separate process that turns those into progressive JPEGs asynchronously
  or via a CDN's operation
  *************************************************************************/
  //jpeg_simple_progression( &cinfo );

  jpeg_start_compress( &cinfo, TRUE );

  // Add an identifying comment
  jpeg_write_marker( &cinfo, JPEG_COM, (const JOCTET*) "Generated by IIPImage", 21 );
  //jpeg_write_marker( &cinfo, JPEG_APP0+1, (const JOCTET*) , )

  // embed icc profile if one was supplied - must be called before writing the scanlines
  if ( icc_profile_buf != NULL )
    write_icc_profile( &cinfo, icc_profile_buf, icc_profile_len );

  // Send the tile data
  unsigned int y;
  int row_stride = width * channels;


  // Try to pass the whole image array at once if it is less than 512x512 pixels:
  // Should be faster than scanlines.
  if( (row_stride * height) <= (512*512*channels) ){

    JSAMPROW *array = new JSAMPROW[height];
    for( y=0; y < height; y++ ){
      array[y] = &data[ y * row_stride ];
    }
    jpeg_write_scanlines( &cinfo, array, height );
    delete[] array;

  }
  else{
    JSAMPROW row[1];
    while( cinfo.next_scanline < cinfo.image_height ) {
      row[0] = &data[ cinfo.next_scanline * row_stride ];
      jpeg_write_scanlines( &cinfo, row, 1 );
    }
  }


  // Tidy up, get the compressed data size and de-allocate memory
  jpeg_finish_compress( &cinfo );

  // Check that we have enough memory in our tile for the JPEG data.
  // This can happen on small tiles with high quality factors. If so
  // delete and reallocate memory.
  y = dest->size;
  if( y > rawtile.width*rawtile.height*rawtile.channels ){
    delete[] (unsigned char*) rawtile.data;
    rawtile.data = new unsigned char[y];
  }

  // Copy memory back to the tile
  memcpy( rawtile.data, dest->source, y );
  delete[] dest->source;
  jpeg_destroy_compress( &cinfo );


  // Set the tile compression parameters
  rawtile.dataLength = y;
  rawtile.compressionType = JPEG;
  rawtile.quality = Q;


  // Return the size of the data we have compressed
  return y;

}



void JPEGCompressor::addXMPMetadata( const string& metadata ){
  jpeg_write_marker( &cinfo, JPEG_APP0, (const JOCTET*) metadata.c_str(), metadata.size() );
}
