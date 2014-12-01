#!/bin/sh

# Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#############################################################
# This script created by Jonas does the following	    #
# Cleans up clones and pevious builds, pulls new clones,    #
# builds, deploys, configures the tests and launches ATRT   #
#############################################################

###############
#Script setup #
##############

save_args=$*
VERSION="autotest-boot.sh version 1.01"

DATE=`date '+%Y-%m-%d'`
if [ `uname -s` != "SunOS" ]
then
  if [ `uname | grep -ic cygwin || true` -ne 0 ]
  then
    HOST=`hostname`
  else
    HOST=`hostname -s`
  fi
else
  HOST=`hostname`
fi
export DATE HOST

set -e

echo "`date` starting: $*"

verbose=0
do_clone=yes
build=yes

patch0=
patch1=
tag0=
tag1=
conf=
clonename=
extra_args=
extra_clone0=
extra_clone1=
install_dir0=
install_dir1=
RUN=

LOCK=$HOME/.autotest-lock

############################
# Read command line entries#
############################

while [ "$1" ]
do
        case "$1" in
                --no-clone) do_clone="";;
                --no-build) build="";;
                --verbose) verbose=`expr $verbose + 1`;;
                --clone=*) clone0=`echo $1 | sed s/--clone=//`;;
                --clone0=*) clone0=`echo $1 | sed s/--clone0=//`;;
                --clone1=*) clone1=`echo $1 | sed s/--clone1=//`;;
                --version) echo $VERSION; exit;;
                --conf=*) conf=`echo $1 | sed s/--conf=//`;;
	        --tag=*) tag0=`echo $1 | sed s/--tag=//`;;
	        --tag0=*) tag0=`echo $1 | sed s/--tag0=//`;;
	        --tag1=*) tag1=`echo $1 | sed s/--tag1=//`;;
	        --clonename=*) clonename=`echo $1 | sed s/--clonename=//`;;
	        --patch=*) patch=`echo $1 | sed s/--patch=//` ; patch0="$patch0$patch " ; patch1="$patch1$patch " ;;
	        --patch0=*) patch=`echo $1 | sed s/--patch0=//` ; patch0="$patch0$patch " ;;
	        --patch1=*) patch=`echo $1 | sed s/--patch1=//` ; patch1="$patch1$patch " ;;
	        --*) echo "Unknown arg: $1";;
                *) RUN="$RUN $1";;
        esac
        shift
done

#################################
#Make sure the configfile exists#
#if it does not exit. if it does#
# (.) load it			# 
#################################
if [ -z "$conf" ]
then
	if [ -f "`pwd`/autotest.conf" ]
	then
		conf="`pwd`/autotest.conf"
	elif [ -f "$HOME/autotest.conf" ]
	then
		conf="$HOME/autotest.conf"
	fi
fi

if [ -f $conf ]
then
	. $conf
else
	echo "Can't find config file: >$conf<"
	exit
fi

###############################
# Validate that all interesting
#   variables where set in conf
###############################
vars="install_dir build_dir bzr_src_base"
for i in $vars
do
  t=`echo echo \\$$i`
  if [ -z "`eval $t`" ]
  then
      echo "Invalid config: $conf, variable $i is not set"
      exit
  fi
done

###############################
#Print out the enviroment vars#
###############################

if [ $verbose -gt 0 ]
then
	env
fi

####################################
# Setup the lock file name and path#
# Setup the clone source location  #
####################################

src_clone0=${bzr_src_base}/${clone0}
src_clone1=${bzr_src_base}/${clone1}

if [ -z "$clone1" ]
then
    install_dir0=$install_dir
else
    install_dir0=$install_dir/0
    install_dir1=$install_dir/1
fi

#######################################
# Check to see if the lock file exists#
# If it does exit. 		      #
#######################################

if [ -f $LOCK ]
then
	echo "Lock file exists: $LOCK"
	exit 1
fi

#######################################
# If the lock file does not exist then#
# create it with date and run info    #
#######################################

echo "$DATE $RUN" > $LOCK

#############################
#If any errors here down, we#
# trap them, and remove the #
# Lock file before exit     #
#############################
if [ `uname -s` != "SunOS" ]
then
	trap "rm -f $LOCK" ERR
fi

# You can add more to this path#
################################

if [ -z "$tag0" ]
then
    dst_place0=${build_dir}/clone-$clone0-$DATE.$$
else
    dst_place0=${build_dir}/clone-$tag0-$DATE.$$
    extra_args="$extra_args --clone0=$tag0"
    extra_clone0="-r$tag0"
fi

if [ -z "$tag1" ]
then
    dst_place1=${build_dir}/clone1-$clone1-$DATE.$$
else
    dst_place1=${build_dir}/clone1-$tag1-$DATE.$$
    extra_args="$extra_args --clone1=$tag1"
    extra_clone1="-r$tag1"
fi

if [ "$clonename" ]
then
    extra_args="$extra_args --clonename=$clonename"
fi

#########################################
# Delete source and pull down the latest#
#########################################

if [ "$do_clone" ]
then
	rm -rf $dst_place0
	mkdir -p ${build_dir}
	
	# Can use local copy if clone_dir is set, to speed up
	if [ ! -z "$clone_dir" ]
	then
		if [ -d "$clone_dir/$clone0" ]
		then
			echo "Copying $clone_dir/$clone0 to $dst_place0"
			cp -r $clone_dir/$clone0 $dst_place0
		fi
	else
		bzr export $dst_place0 $extra_clone0 $src_clone0
		for patch in $patch0 ; do
			( cd $dst_place0 && patch -p0 ) < $patch
		done
                {
	          bzr version-info $extra_clone0 $src_clone0
	          if [ $patch0 ] ; then echo patches: $patch0 ; cat $patch0 ; fi
                } > $dst_place0/code0.txt
	fi

	if [ "$clone1" ]
	then
	    rm -rf $dst_place1
	    bzr export $dst_place1 $extra_clone1 $src_clone1
	    for patch in $patch1 ; do
		( cd $dst_place1 && patch -p0 ) < $patch
	    done
            {
	      bzr version-info $extra_clone1 $src_clone1
	      if [ $patch1 ] ; then echo patches: $patch1 ; cat $patch1 ; fi
            } > $dst_place1/code1.txt
	fi
fi

##########################################
# Build the source, make installs, and   #
# create the database to be rsynced	 #
##########################################

function build_cluster()
{
    if grep -qc autotest storage/ndb/compile-cluster 2>/dev/null
    then
        storage/ndb/compile-cluster --autotest $*
    else
        BUILD/compile-ndb-autotest $*
    fi
}

if [ "$build" ]
then
    rm -rf $install_dir
    p=`pwd`
    if [ -z "$clone1" ]
    then
        cd $dst_place0
        if [ `uname | grep -ic cygwin || true` -ne 0 ]
        then
            install_dir_dos=`cygpath -w $install_dir`
            cmd /c cscript win/configure.js WITH_NDBCLUSTER_STORAGE_ENGINE --without-plugins=archive,blackhole,example,federated
            cmd /c cmake -G "Visual Studio 9 2008" -DWITH_ERROR_INSERT=1 -DWITH_NDB_TEST=1 -DCMAKE_INSTALL_PREFIX=$install_dir_dos
            cmd /c devenv.com MySql.sln /Build RelWithDebInfo
            cmd /c devenv.com MySql.sln /Project INSTALL /Build
        else
	    build_cluster --prefix=$install_dir0
	    make install
	    [ ! -f code0.txt ] || cp code0.txt $install_dir0/
        fi
    else
	cd $dst_place0
	build_cluster --prefix=$install_dir0
	make install
	[ ! -f code0.txt ] || cp code0.txt $install_dir0/
	
	cd $dst_place1
	build_cluster --prefix=$install_dir1
	make install
	[ ! -f code1.txt ] || cp code1.txt $install_dir1/
    fi
    cd $p
fi


################################
# Start run script             #
################################

script=$install_dir0/mysql-test/ndb/autotest-run.sh
for R in $RUN
do
    sh -x $script $save_args --conf=$conf --run-dir=$install_dir --install-dir0=$install_dir0 --install-dir1=$install_dir1 --suite=$R --nolock $extra_args
done

if [ "$build" ]
then
    rm -rf $dst_place0

    if [ "$dst_place1" ]
    then
	rm -rf $dst_place1
    fi
fi

rm -f $LOCK
