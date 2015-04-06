#!/bin/bash
###########################
# Builds one specific MWM #
###########################

# Prerequisites:
#
# - The script should be placed in omim/tools/unix, or supply OMIM_PATH with path to omim repo
# - Data path with classificators etc. should be present in $OMIM_PATH/data
#     Inside it should be at least: categories.txt, classificator.txt, types.txt, drules_proto.bin
# - Compiled generator_tool somewhere in omim/../build/out/whatever, or supply BUILD_PATH
# - For routing, compiled OSRM binaries in omim/3party/osrm/osrm-backend/build, or supply OSRM_BUILD_PATH
# - Target path for mwm is the same as o5m path, or supply TARGET

# Cross-borders routing index is not created, since we don't assume
# the source file to be one of the pre-defined countries.

if [ $# -lt 1 ]; then
  echo ''
  echo "Usage: $0 \<file.o5m/bz2\>"
  echo "To build routing: $0 \<file.o5m/bz2\> \<profile.lua\>"
  echo ''
  exit 0
fi

fail() {
  [ -n "$INTDIR" ] && rm -r "$INTDIR"
  [ -n "$1" ] && echo "$1" >&2
  exit 1
}
trap fail SIGINT SIGTERM

SOURCE_FILE="$1"
SOURCE_TYPE="${1##*.}"
BASE_NAME="${SOURCE_FILE%%.*}"
[ -z "$TARGET" ] && TARGET="$(dirname "$SOURCE_FILE")"
[ ! -d "$TARGET" ] && fail "$TARGET should be a writable folder"
[ -z "$OMIM_PATH" ] && OMIM_PATH="$(cd "$(dirname "$0")/../.."; pwd)"
DATA_PATH="$OMIM_PATH/data/"
[ ! -r "${DATA_PATH}types.txt" ] && fail "Cannot find classificators in $DATA_PATH, please set correct OMIM_PATH"

if [ $# -gt 1 ]; then
  MODE=routing
else
  MODE=mwm
fi

if [ -z "$GENERATOR_TOOL" ]; then
  # find generator_tool. Supply your own priority dir if needed
  IT_PATHS_ARRAY=()
  for i in $BUILD_PATH $OMIM_PATH $OMIM_PATH/../*omim*elease* $OMIM_PATH/../*omim*ebug; do
    if [ -d "$i/out" ]; then
      IT_PATHS_ARRAY+=("$i/out/release/generator_tool" "$i/out/debug/generator_tool")
    fi
  done

  for i in "$BUILD_PATH/generator_tool" ${IT_PATHS_ARRAY[@]}; do
    if [ -x "$i" ]; then
      GENERATOR_TOOL="$i"
      break
    fi
  done
fi

[ ! -x "$GENERATOR_TOOL" ] && fail "No generator_tool found in ${IT_PATHS_ARRAY[*]}"
echo "Using tool: $GENERATOR_TOOL"

if [ "$(uname)" == "Darwin" ]; then
  INTDIR=$(mktemp -d -t mwmgen)
else
  INTDIR=$(mktemp -d)
fi

if [ "$MODE" == "mwm" ]; then
  # Create MWM file
  INTDIR_FLAG="--intermediate_data_path=$INTDIR/ --node_storage=map"
  GENERATE_EVERYTHING='--generate_features=true --generate_geometry=true --generate_index=true --generate_search_index=true'
  if [ "$SOURCE_TYPE" == "o5m" ]; then
    INTDIR_FLAG="$INTDIR_FLAG --osm_file_type=o5m --osm_file_name=$SOURCE_FILE"
    $GENERATOR_TOOL $INTDIR_FLAG --preprocess=true || fail "Preprocessing failed"
    $GENERATOR_TOOL $INTDIR_FLAG --data_path="$TARGET" --user_resource_path="$DATA_PATH" $GENERATE_EVERYTHING --output="$BASE_NAME"
  elif [ "$SOURCE_TYPE" == "bz2" ]; then
    bzcat "$SOURCE_FILE" | $GENERATOR_TOOL $INTDIR_FLAG --preprocess=true || fail "Preprocessing failed"
    bzcat "$SOURCE_FILE" | $GENERATOR_TOOL $INTDIR_FLAG --data_path="$TARGET" --user_resource_path="$DATA_PATH" $GENERATE_EVERYTHING --output="$BASE_NAME"
  else
    fail "Unsupported source type: $SOURCE_TYPE"
  fi

elif [ "$MODE" == "routing" ]; then
  # Create .mwm.routing file
  [ -z "$OSRM_PATH" ] && OSRM_PATH="$OMIM_PATH/3party/osrm/osrm-backend"
  [ -z "$OSRM_BUILD_PATH" ] && OSRM_BUILD_PATH="$OSRM_PATH/build"
  [ ! -x "$OSRM_BUILD_PATH/osrm-extract" ] && fail "Please compile OSRM binaries to $OSRM_BUILD_PATH"
  [ ! -r "$TARGET/$BASE_NAME.mwm" ] && fail "Please build mwm file beforehand"

  [ -z "$OSRM_THREADS" ] && OSRM_THREADS=15
  [ -z "$OSRM_MEMORY" ] && OSRM_MEMORY=50
  EXTRACT_CFG="$INTDIR/extractor.ini"
  PREPARE_CFG="$INTDIR/contractor.ini"
  echo "threads = $OSRM_THREADS" > "$EXTRACT_CFG"
  echo "memory = $OSRM_MEMORY" > "$PREPARE_CFG"
  echo "threads = $OSRM_THREADS" >> "$PREPARE_CFG"
  if [ -r "$2" ]; then
    PROFILE="$2"
  else
    echo "$2 is not a profile, using standard car.lua" >&2
    PROFILE="$OSRM_PATH/profiles/car.lua"
  fi
  [ ! -r "$PROFILE" ] && fail "Lua profile $PROFILE is not found"

  PBF="$INTDIR/$BASENAME.pbf"
  OSRM="$INTDIR/$BASENAME.osrm"
  export STXXLCFG=~/.stxxl
  # just a guess
  OSMCONVERT=~/osmctools/osmconvert
  if [ ! -x $OSMCONVERT ]; then
    OSMCONVERT="$INTDIR/osmconvert"
    wget -O - http://m.m.i24.cc/osmconvert.c | cc -x c - -lz -O3 -o $OSMCONVERT
  fi
  if [ "$SOURCE_TYPE" == "bz2" ]; then
    bzcat "$SOURCE_FILE" | $OSMCONVERT - -o=$PBF || fail "Converting to PBF failed"
  else
    $OSMCONVERT "$SOURCE_FILE" -o=$PBF || fail "Converting to PBF failed"
  fi
  "$OSRM_BUILD_PATH/osrm-extract" --config "$EXTRACT_CFG" --profile "$PROFILE" "$PBF" || fail
  rm "$PBF"
  "$OSRM_BUILD_PATH/osrm-prepare" --config "$PREPARE_CFG" --profile "$PROFILE" "$OSRM" || fail
  "$OSRM_BUILD_PATH/osrm-mapsme" -i "$OSRM" || fail
  # create fake poly file
  POLY="$TARGET/borders/$BASE_NAME.poly"
  if [ ! -r "$POLY" ]; then
    POLY_DIR="$(dirname "$POLY")"
    mkdir -p "$POLY_DIR"
    cat > "$POLY" <<EOPOLY
fake
1
	-180.0	-90.0
	-180.0	90.0
	180.0	90.0
	180.0	-90.0
	-180.0	-90.0
END
END
EOPOLY
    if [ -r "$TARGET/polygons.lst" ]; then
      mv "$TARGET/polygons.lst" "$POLY_DIR"
    fi
    echo "$BASE_NAME" > "$TARGET/polygons.lst"
  fi
  $GENERATOR_TOOL --osrm_file_name="$OSRM" --data_path="$TARGET" --user_resource_path="$DATA_PATH" --output="$BASE_NAME"
  if [ -n "$POLY_DIR" ]; then
    # remove fake poly
    rm "$POLY"
    rm "$TARGET/polygons.lst"
    if [ -r "$POLY_DIR/polygons.lst" ]; then
      mv "$POLY_DIR/polygons.lst" "$TARGET"
    fi
    if [ -z "$(ls -A "$POLY_DIR")" ]; then
      rm -r "$POLY_DIR"
    fi
  fi
fi

rm -r "$INTDIR"
