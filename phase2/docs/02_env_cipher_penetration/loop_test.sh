#!/bin/sh

for id_num in $(seq 1 1 37)
do
  echo id_num: $id_num
  ./name_decript.sh $id_num
  sed '$ s/\x00*$//' name${id_num} >> names
#   cat name${id_num} >> names
  echo " (id_num:"${id_num}")" >> names
  rm name${id_num}
  ./blob_decript.sh $id_num
done

mkdir result
mv blob*.jpg result
mv names result
