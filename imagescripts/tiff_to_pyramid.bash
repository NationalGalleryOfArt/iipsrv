#!/bin/bash

# Error handling function
function handle_error() {
    /bin/echo "Script exited with status ${2} at line ${1}"
    if [ -z "${savefiles}" ]; then /bin/rm -f $tmpprefix*; fi
    if [ -z "${savefiles}" ]; then /bin/rm -f $outprefix*; fi
}

# Trap any error and invoke handler
trap 'handle_error ${LINENO} $?' ERR
set -e  # Exit on any error

# Input arguments
if [[ -z "$1" || -z "$2" || -z "$3" || -z "$4" ]]; then
    /bin/echo "Usage: tiff_to_pyramid.bash <tmpdir> <input> <output> <max pixels or 'none'> <savefiles (optional)>"
    exit 1
fi

if [[ ! -d $1 || ! -f $2 ]]; then
    /bin/echo "Either tmpdir or input file does not exist."
    exit 1
fi

if [ -f $3 ]; then
    /bin/echo "The output file already exists! Deleting it."
    if [ -z "${savefiles}" ]; then /bin/rm -f $3; fi
fi

/bin/touch $3 || { echo "Could not create the output file."; exit 1; }
/bin/rm -f $3  # Clean slate

# Setup variables
tmpdir=$1
input=$2
outfile=$3
maxpixels=$4
savefiles=$5
pid=$$
tmpprefix=${tmpdir}/stripped_srgb_${pid}
outprefix=${tmpdir}/srgb_${pid}

[[ ${maxpixels} == "none" ]] && unset maxpixels

# Cleanup pre-existing temp files
[ -z "${savefiles}" ] && /bin/rm -f $tmpprefix* $outprefix*

# Detect image properties
CHANNELS=$(/usr/local/nga/bin/images/utils/pydentify.sh ${input} 2>/dev/null)
BIT_DEPTH=$(/usr/bin/identify -format "%[depth]\n" ${input}[0] 2>/dev/null)
COLORSPACE=$(/usr/bin/identify -format "%[colorspace]\n" ${input}[0] 2>/dev/null)

echo "channels: ${CHANNELS}"
echo "bit depth: ${BIT_DEPTH}"
echo "colorspace: ${COLORSPACE}"

# Special case: 16-bit grayscale image
if [[ ${COLORSPACE} == "Gray" && ${BIT_DEPTH} == "16" ]]; then
    echo "Converting 16-bit grayscale to 8-bit..."
    /usr/local/bin/vips linear ${input} ${input}.scaled.tif 0.0038910506 0
    /usr/local/bin/vips cast ${input}.scaled.tif ${input}.8bit.tif uchar
    /bin/mv ${input}.8bit.tif ${input}
    /bin/rm -f ${input}.scaled.tif
    CHANNELS=$(/usr/local/nga/bin/images/utils/pydentify.sh ${input} 2>/dev/null)
    BIT_DEPTH=$(/usr/bin/identify -format "%[depth]\n" ${input}[0] 2>/dev/null)
    COLORSPACE=$(/usr/bin/identify -format "%[colorspace]\n" ${input}[0] 2>/dev/null)
    echo "Updated: channels=${CHANNELS}, bit depth=${BIT_DEPTH}, colorspace=${COLORSPACE}"
fi

# Remove alpha if present
if [[ ${CHANNELS} == *"rgba"* || ${CHANNELS} == *"srgba"* ]]; then
    echo "Removing alpha channel from $input"
    /usr/local/bin/vips im_extract_bands $input ${input}.noalpha.tif 0 3
    [ -z "${savefiles}" ] && /bin/rm $input
    /bin/mv ${input}.noalpha.tif $input
elif [[ ${CHANNELS} != *"srgb"* && ${CHANNELS} != *"gray"* && ${CHANNELS} != *"cmyk"* ]]; then
    echo "Unsupported channel type: ${CHANNELS}"
    exit 1
fi

# Embed sRGB ICC profile for gray images missing profile
if [[ ${CHANNELS} == *"gray"* ]]; then
    ICCPROFILE=$(/usr/bin/identify -format "%[profile:icc]\n" ${input}[0] 2>/dev/null)
    if [ -z "${ICCPROFILE}" ] || [ "${ICCPROFILE}" == "sRGB Profile" ]; then
        W2=$(/usr/local/bin/vipsheader -f width ${input}[0])
        H2=$(/usr/local/bin/vipsheader -f height ${input}[0])
        /usr/local/bin/vipsthumbnail $input[0] --eprofile=/usr/local/nga/etc/sRGBProfile.icc --size ${W2}x${H2} --intent perceptual -o ${tmpprefix}.tif[compression=none,strip]
    fi
fi

# General ICC conversion fallback
if [ -z "${W2}" ]; then
    /usr/local/bin/vips icc_transform $input ${tmpprefix}.tif[compression=none,strip] /usr/local/nga/etc/sRGBProfile.icc --embedded --input-profile /usr/local/nga/etc/sRGBProfile.icc --intent perceptual
fi

# Remove alpha again if needed and copy with sRGB interpretation
NUM_BANDS=$(/usr/local/bin/vipsheader -f bands "${tmpprefix}.tif")
if [[ "$NUM_BANDS" == "4" ]]; then
    /usr/local/bin/vips extract_band "${tmpprefix}.tif" "${tmpprefix}_3band.tif" 0 --n 3
    SRC_FOR_COPY="${tmpprefix}_3band.tif"
else
    SRC_FOR_COPY="${tmpprefix}.tif"
fi

/usr/local/bin/vips copy "$SRC_FOR_COPY" "${tmpprefix}_rgb.tif" --interpretation srgb
/usr/local/bin/vips tiffsave "${tmpprefix}_rgb.tif" "${outprefix}_0.tif" --compression none --profile /usr/local/nga/etc/sRGBProfile.icc

# Read dimensions of final base image
W=$(/usr/local/bin/vipsheader -f width "${tmpprefix}_rgb.tif")
H=$(/usr/local/bin/vipsheader -f height "${tmpprefix}_rgb.tif")

# Copy first tile
/bin/tiffcp -c jpeg:90 -t -w 256 -l 256 "${outprefix}_0.tif" -a "${outfile}"

# Prepare optional restricted resolution
if [[ ! -z ${maxpixels} && ${maxpixels} != "0" ]]; then
    if (( W > H )); then
        RW=$maxpixels
        RH=$(( ($maxpixels * $H + $W / 2) / $W ))
    else
        RH=$maxpixels
        RW=$(( ($maxpixels * $W + $H / 2) / $H ))
    fi
fi

# Generate pyramid levels
c=0; rc=0
while true; do
    W=$(( W / 2 ))
    H=$(( H / 2 ))
    /bin/echo ${c} ${W} ${H}
    (( RW > 0 && RH > 0 && W >= 2*RW && H >= 2*RH )) && rc=$c
    /usr/local/bin/vipsthumbnail ${outprefix}_$c.tif --size ${W}x${H}\! -o ${outprefix}_$(( c + 1 )).tif[compression=none]
    /bin/tiffcp -c jpeg:90 -t -w 256 -l 256 ${outprefix}_$(( c + 1 )).tif -a ${outfile}
    c=$(( c + 1 ))
    (( W < 1 || H < 1 || (W < 129 && H < 129) )) && break
done

# Create restricted-res pyramid tiles
if (( RW > 0 && RH > 0 )); then
    while true; do
        /bin/echo ${c} from:${rc} ${RW} ${RH}
        /usr/local/bin/vipsthumbnail ${outprefix}_$rc.tif --size ${RW}x${RH}\! -o ${outprefix}_$(( c + 1 )).tif[compression=none]
        /bin/tiffcp -c jpeg:90 -t -w 256 -l 256 ${outprefix}_$(( c + 1 )).tif -a ${outfile}
        c=$(( c + 1 )); rc=$(( rc + 1 ))
        RW=$(( RW / 2 )); RH=$(( RH / 2 ))
        (( RW < 1 || RH < 1 || (RW < 129 && RH < 129) )) && break
    done
fi

# Embed restriction tag
META_OK=1
if [ ! -z $maxpixels ]; then
    /usr/local/nga/bin/images/exif_write_max_pixels.pl ${outfile} $maxpixels || {
        echo "Problem writing restricted image metadata"
        META_OK=0
    }
fi

# Clean up temp files
[ -z "${savefiles}" ] && /bin/rm -f $tmpprefix* $outprefix*

# Final sanity check
WF=$(/usr/local/bin/vipsheader -f width $outfile)
HF=$(/usr/local/bin/vipsheader -f height $outfile)

echo "Pyramid width: ${WF}"
echo "Pyramid height: ${HF}"

# Exit 0 only if everything succeeded
exit $(( WF == W && HF == H && META_OK == 1 ))
