#!/bin/bash
# creat_rpm.sh 

if [ $# != 1 ]; then
	echo "Usage: ./create_rpm.sh  pathname_of_RPM_BUILD_ROOT"
	exit 1
fi

BUILD_ROOT=${1}

CSA_VERSION="5.0.0"
JOB_VERSION="3.0.0"

tar -cjpf ${BUILD_ROOT}/SOURCES/job-${JOB_VERSION}.tar.bz2 job
tar -cjpf ${BUILD_ROOT}/SOURCES/csa-${CSA_VERSION}.tar.bz2 csa

rpmbuild -ba job.spec
if test $? -ne 0
then
	echo "Failed to build job rpms"
	exit 1
fi

rpmbuild -ba csa.spec
if test $? -ne 0; then
	echo "Failed to build csa rpms"
	exit 1
fi
exit 0
