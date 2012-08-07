#!/bin/sh
#
# Execute this tool to setup the environment and build binary releases
# for Percona-Server starting from a fresh tree.
#
# Usage: build-binary.sh [target dir]
# The default target directory is the current directory. If it is not
# supplied and the current directory is not empty, it will issue an error in
# order to avoid polluting the current directory after a test run.
#

# Bail out on errors, be strict
set -ue

# Examine parameters
TARGET="$(uname -m)"
TARGET_CFLAGS=''
QUIET='VERBOSE=1'
NOHS='no'

# Some programs that may be overriden
TAR=${TAR:-tar}

# Check if we have a functional getopt(1)
if ! getopt --test
then
    go_out="$(getopt --options="iqH" --longoptions=i686,quiet,nohs \
        --name="$(basename "$0")" -- "$@")"
    test $? -eq 0 || exit 1
    eval set -- $go_out
fi

for arg
do
    case "$arg" in
    -- ) shift; break;;
    -i | --i686 )
        shift
        TARGET="i686"
        TARGET_CFLAGS="-m32 -march=i686"
        ;;
    -q | --quiet )
        shift
        QUIET=''
        ;;
    -H | --nohs )
        shift
        NOHS='yes'
        ;;
    esac
done

# Working directory
if test "$#" -eq 0
then
    WORKDIR="$(pwd)"
    
    # Check that the current directory is not empty
    if test "x$(echo *)" != "x*"
    then
        echo >&2 \
            "Current directory is not empty. Use $0 . to force build in ."
        exit 1
    fi

elif test "$#" -eq 1
then
    WORKDIR="$1"

    # Check that the provided directory exists and is a directory
    if ! test -d "$WORKDIR"
    then
        echo >&2 "$WORKDIR is not a directory"
        exit 1
    fi

    WORKDIR_ABS="$(cd "$WORKDIR"; pwd)"

else
    echo >&2 "Usage: $0 [target dir]"
    exit 1

fi

SOURCEDIR="$(cd $(dirname "$0"); cd ..; pwd)"
test -e "$SOURCEDIR/Makefile" || exit 2

# Extract version from the Makefile
MYSQL_VERSION="$(grep ^MYSQL_VERSION= "$SOURCEDIR/Makefile" \
    | cut -d = -f 2)"
PERCONA_SERVER_VERSION="$(grep ^PERCONA_SERVER_VERSION= \
    "$SOURCEDIR/Makefile" | cut -d = -f 2)"
PRODUCT="Percona-Server-$MYSQL_VERSION-$PERCONA_SERVER_VERSION"

# Build information
REVISION="$(cd "$SOURCEDIR"; bzr revno)"
PRODUCT_FULL="Percona-Server-$MYSQL_VERSION-$PERCONA_SERVER_VERSION"
PRODUCT_FULL="$PRODUCT_FULL-$REVISION.$(uname -s).$TARGET"
COMMENT="Percona Server with XtraDB (GPL), Release $PERCONA_SERVER_VERSION"
COMMENT="$COMMENT, Revision $REVISION"

# Compilation flags
export CC=${CC:-gcc}
export CXX=${CXX:-g++}
export CFLAGS="-fPIC -Wall -O3 -g -static-libgcc -fno-omit-frame-pointer -DPERCONA_INNODB_VERSION=$PERCONA_SERVER_VERSION $TARGET_CFLAGS ${CFLAGS:-}"
export CXXFLAGS="-O2 -fno-omit-frame-pointer -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2 -fno-exceptions -DPERCONA_INNODB_VERSION=$PERCONA_SERVER_VERSION $TARGET_CFLAGS ${CXXFLAGS:-}"
export MAKE_JFLAG=-j4

# Create a temporary working directory
INSTALLDIR="$(cd "$WORKDIR" && TMPDIR="$WORKDIR_ABS" mktemp -d percona-build.XXXXXX)"
INSTALLDIR="$WORKDIR_ABS/$INSTALLDIR"   # Make it absolute

# Build
(
    cd "$SOURCEDIR"
 
    # Execute clean and download mysql, apply patches
    make clean all

    cd "$PRODUCT"
    cmake . -DBUILD_CONFIG=mysql_release \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DWITH_EMBEDDED_SERVER=OFF \
        -DFEATURE_SET=community \
        -DENABLE_DTRACE=OFF \
        -DCMAKE_INSTALL_PREFIX="/usr/local/$PRODUCT_FULL" \
        -DMYSQL_DATADIR="/usr/local/$PRODUCT_FULL/data" \
        -DMYSQL_SERVER_SUFFIX="-$PERCONA_SERVER_VERSION" \
        -DCOMPILATION_COMMENT="$COMMENT"

    make $MAKE_JFLAG $QUIET
    make DESTDIR="$INSTALLDIR" install

    if test "x$NOHS" == "xno"
    then
    # Build HandlerSocket
    (
        cd "storage/HandlerSocket-Plugin-for-MySQL"
        ./autogen.sh
        CXX=${HS_CXX:-g++} ./configure --with-mysql-source="$SOURCEDIR/$PRODUCT" \
            --with-mysql-bindir="$SOURCEDIR/$PRODUCT/scripts" \
            --with-mysql-plugindir="/usr/local/$PRODUCT_FULL/lib/mysql/plugin" \
            --libdir="/usr/local/$PRODUCT_FULL/lib/mysql/plugin" \
            --prefix="/usr/local/$PRODUCT_FULL"
        make
        make DESTDIR="$INSTALLDIR" install

    )
    fi

    # Build UDF
    (
        cd "UDF"
        CXX=${UDF_CXX:-g++} ./configure --includedir="$SOURCEDIR/$PRODUCT/include" \
            --libdir="/usr/local/$PRODUCT_FULL/mysql/plugin"
        make
        make DESTDIR="$INSTALLDIR" install

    )

)

# Package the archive
(
    cd "$INSTALLDIR/usr/local/"

    $TAR czf "$WORKDIR_ABS/$PRODUCT_FULL.tar.gz" \
        --owner=0 --group=0 "$PRODUCT_FULL/"
    
)

# Clean up
rm -rf "$INSTALLDIR"

