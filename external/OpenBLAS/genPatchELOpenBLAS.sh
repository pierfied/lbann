#!/bin/sh

# Run this under Elemental's source directory with,
# for example, fileToPatch listed in 'ChangedFiles.txt' and
# fileToPatch.new placed under the directory where this script exists

thisCommand=`basename $0`
fileList=`echo $0 | sed 's/'${thisCommand}'/ChangedFiles.EL_OpenBLAS.txt/'`
patchDir=`echo $0 | sed 's/\/'${thisCommand}'//'`
patchName=EL_OpenBLAS.patch

echo 'fileList = '${fileList}
echo 'patchFile = '${patchDir}/${patchName}

while read f
do
  fdir=`dirname $f`
  forg=`basename $f`
  fnew=${forg}'.new'

  if [ ! -f $f ] || [ ! -f ${patchDir}/${fnew} ] ; then
    continue;
  fi

  ln -s ${patchDir}/${fnew} ${fdir}/${fnew}
  diff -u $f ${fdir}/${fnew}
  rm ${fdir}/${fnew}
done < ${fileList} > ${patchDir}/${patchName}
