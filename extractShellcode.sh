#!/bin/sh

cont=0;

for i in $(objdump -d $1 | grep "^ " | cut -f2);do
	echo -n '\x'$i;
	cont=`expr $cont + 1`;
done;
echo -n '\x00';
cont=`expr $cont + 1`;
echo "\n*** $cont B ***";
