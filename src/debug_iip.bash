#!/bin/bash
export MEMCACHED_SERVERS=localhost:11211
export MEMCACHED_TIMEOUT=300
export ALLOW_UPSCALING=1
export LOGFILE=/var/log/iipsrv/iipsrv.log
export VERBOSITY=3
export FILESYSTEM_PREFIX=/mnt/images/
export JPEG_QUALITY=90
export MAX_CVT=4096
export MAX_HEADERS_IN_METADATA_CACHE=10000
export MAX_IMAGE_CACHE_SIZE=0
export INTERPOLATION=2
export OVERSAMPLING_FACTOR=1.5
export IIIF_PREFIX=/iiif
export RETAIN_SOURCE_ICC_PROFILE=1
#gdb --args ./iipsrv.fcgi /iiif/public/objects/5/1/51-primary-0-nativeres.ptif/full/2048,/0/default.jpg
 ./iipsrv.fcgi /iiif/public/objects/5/1/51-primary-0-nativeres.ptif/full/2048,/0/default.jpg
