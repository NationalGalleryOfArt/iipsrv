#!/bin/bash

function handle_error() {
    /bin/echo "Script exited with status ${2} at line ${1}"
    if [ -z "${savefiles}" ]; then /bin/rm -f $tmpprefix*; fi
    if [ -z "${savefiles}" ]; then /bin/rm -f $outprefix*; fi
}

trap 'handle_error ${LINENO} $?' ERR

# cause the script to fail if any commands exist with non-zero status
set -e

if [[ -z "$1" || -z "$2" || -z "$3" ]]; then
    /bin/echo "Usage:tiff_to_pyramid.bash <tmpdir> <full path to image source> <full path to output target> <save all working files - any value will do>";
    exit 1;
fi
if [[ ! -d $1 || ! -f $2 ]]; then
    /bin/echo "Either the tmpdir or input file do not exist.";
    exit 1;
fi

if [ -f $3 ]; then
    /bin/echo "The output file already exists! Deleting it.";
    if [ -z "${savefiles}" ]; then /bin/rm -f $3; fi
fi

/bin/touch $3;

if [ ! -f $3 ]; then
    /bin/echo "Could not create the output file.";
    exit 1;
fi

/bin/rm -f $3

tmpdir=$1
# only process the first page of a potentially multi-page image document
input=$2
outfile=$3
savefiles=$4
pid=$$;
tmpprefix=${tmpdir}/stripped_srgb_${pid}
outprefix=${tmpdir}/srgb_${pid}

# cleanup temp files that might already exist
if [ -z "${savefiles}" ]; then /bin/rm -f $tmpprefix*; fi
if [ -z "${savefiles}" ]; then /bin/rm -f $outprefix*; fi

# first, use vipsheader to read the bands
CHANNELS=$(/usr/bin/identify -format "%[channels]\n" ${input}[0] 2>/dev/null)
echo "channels: ${CHANNELS}"
if [ ${CHANNELS} = "srgba" ]; then
    # we have to flatten the image to remove the alpha channel / trasparency before proceeding
    echo "removing alpha channel from $input"
    echo "/usr/local/bin/vips im_extract_bands $input ${input}.noalpha.tif 0 3"
    /usr/local/bin/vips im_extract_bands $input ${input}.noalpha.tif 0 3   2>&1
    #echo "/usr/local/bin/vips flatten $input ${input}.noalpha.tif[compression=none] --background 255,255,255"
    #/usr/local/bin/vips flatten $input ${input}.noalpha.tif[compression=none] --background 255,255,255 2>&1
    if [ -z "${savefiles}" ]; then /bin/rm $input; fi
    /bin/mv ${input}.noalpha.tif $input
elif [[ ${CHANNELS} != "srgb" && ${CHANNELS} != "gray" && ${CHANNELS} != "cmyk" ]]; then
    echo "Image ${input} has color channels ${CHANNELS} which is not supported at this time."
    exit 1;
fi

# check for special case of gray colorspace and no embedded profile
if [[ ${CHANNELS} == "gray" ]]; then
    ICCPROFILE=$(/usr/bin/identify -format "%[profile:icc]\n" ${input}[0] 2>/dev/null)
    echo "icc profile description: ${ICCPROFILE}"
    # in the case of gray with no embedded color profile, we can't just apply sRGB with the icc_transform because sRGB isn't an appropriate profile for the icc_transform command
    # so we have to call vipsthumbnail instead which does some magick behind the scenes
    if [ -z "${ICCPROFILE}" ]; then
        W2=$(/usr/local/bin/vipsheader -f width ${input}[0] 2>/dev/null)
        H2=$(/usr/local/bin/vipsheader -f height ${input}[0] 2>/dev/null)
        echo "/usr/local/bin/vipsthumbnail $input[0] --eprofile=/usr/local/nga/etc/sRGBProfile.icc --size ${W2}x${H2} --intent perceptual -o ${tmpprefix}.tif[compression=none,strip]"
        /usr/local/bin/vipsthumbnail $input[0] --eprofile=/usr/local/nga/etc/sRGBProfile.icc --size ${W2}x${H2} --intent perceptual -o ${tmpprefix}.tif[compression=none,strip] 2>&1
        # note that in the above operation, vipsthumbnail doesn't embed the profile by default, so there won't be one in the result since we didn't start with one
    fi
fi

# if we haven't already transformed color profile to sRGB like in above operation for a missing or incompatible image then do it now
if [ -z "${W2}" ]; then
    # next, run an icc_transform to convert the original to sRGB (assume sRGB if no profile and otherwise use the embedded one) 
    # and strip all metadata from the file; --embedded intructs vips to use embedded and --input-profile is only used as a fallback 
    # if a profile isn't embedded
    echo "/usr/local/bin/vips icc_transform $input ${tmpprefix}.tif[compression=none,strip] /usr/local/nga/etc/sRGBProfile.icc --embedded --input-profile /usr/local/nga/etc/sRGBProfile.icc"
    /usr/local/bin/vips icc_transform $input ${tmpprefix}.tif[compression=none,strip] /usr/local/nga/etc/sRGBProfile.icc --embedded --input-profile /usr/local/nga/etc/sRGBProfile.icc --intent perceptual 2>&1 
fi

# now, embed an sRGB ICC profile in the resulting uncompressed tiff since we stripped out the profile above using the strip metadata directive
# it would be nice if there was a way to do this during the icc_transform step, but there doesn't seem to be
echo "/usr/local/bin/vips tiffsave ${tmpprefix}.tif ${outprefix}_0.tif --compression none --profile /usr/local/nga/etc/sRGBProfile.icc"
/usr/local/bin/vips tiffsave ${tmpprefix}.tif ${outprefix}_0.tif --compression none --profile /usr/local/nga/etc/sRGBProfile.icc 2>&1

# read the width and height of the transformed file
W=$(/usr/local/bin/vipsheader -f width ${outprefix}_0.tif)
H=$(/usr/local/bin/vipsheader -f height ${outprefix}_0.tif)
c=0;
while [ 1 ]; do
    W=$(( W / 2 ));
    H=$(( H / 2 ));
    #/bin/echo ${c} ${W} ${H}

    # since we already have a stripped and color transformed tiff that 
    # is twice the resolution as the one are about to create, use that 
    # one instead of the original when resizing which will save quite 
    # a bit of processing time
    echo "/usr/local/bin/vipsthumbnail ${outprefix}_$c.tif --size ${W}x${H}\! -o ${outprefix}_$(( c + 1 )).tif[compression=none] "
    /usr/local/bin/vipsthumbnail ${outprefix}_$c.tif --size ${W}x${H}\! -o ${outprefix}_$(( c + 1 )).tif[compression=none] 2>&1 

    # reduce height and width by half and repeat process until small enough
    if (( W < 1 || H < 1 || (( W < 129 && H < 129 )) )); then
        break;
    fi
    c=$(( c + 1 ));
done

# once we have all sizes created, then we use tiffcp to perform the pyramid 
# assembly and jpeg compression at 90 quality

# note: tiffcp defaults to ycbcr photometric interpretation and presumably therefore 
# chroma subsampling so by default it produces JPEGs about 3x smaller with ycbcr 
# vs. when photometric interpretation is set to rgb
# e.g. -c jpeg:r:90 vs. -c jpeg:90 
echo "/bin/tiffcp -c jpeg:90 -t -w 256 -l 256 ${outprefix}_*.tif -a ${outfile} "
/bin/tiffcp -c jpeg:90 -t -w 256 -l 256 ${outprefix}_*.tif -a ${outfile} 2>&1 

# cleanup temp files but leave the outputput file in place
if [ -z "${savefiles}" ]; then /bin/rm -f $tmpprefix*; fi
if [ -z "${savefiles}" ]; then /bin/rm -f $outprefix*; fi

# sanity check
WF=$(/usr/local/bin/vipsheader -f width $outfile)
HF=$(/usr/local/bin/vipsheader -f height $outfile)

/bin/echo "Pyramid width: ${WF}"
/bin/echo "Pyramid height: ${HF}"

# final check
exit $(( WF == W && HF == H ))

